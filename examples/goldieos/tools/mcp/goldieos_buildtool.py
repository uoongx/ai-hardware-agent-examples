#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
项目编译 MCP 服务器 - 简化版
功能：
1. 设置项目目录
2. 代码编译（同步等待完成）
3. 获取编译进度
4. 获取编译log
"""

import asyncio
import json
import sys
import os
import subprocess
import re
import logging
from datetime import datetime
from typing import Dict, Any, List, Optional

# 设置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stderr),
        logging.FileHandler('e:\\mcp_build_log.txt', encoding='utf-8')
    ]
)
logger = logging.getLogger("build-mcp-server")

class BuildManager:
    """编译管理器 - 简化版"""
    
    def __init__(self):
        self.project_dir = None
        self.build_command = None
        self.log_command = None
        self.is_building = False
        self.build_progress = 0
        self.build_status = "idle"  # idle, building, success, error
        self.last_error = ""
        self.log_file = "e:\\mcp_data\\build.log"
        self.config_file = "e:\\mcp_data\\build_mcp_config.txt"
        self.build_start_time = None
        self.build_end_time = None
        
        # 确保目录存在
        os.makedirs("e:\\mcp_data", exist_ok=True)
        
        # 初始化时读取配置文件
        self._load_config()
    
    def _load_config(self):
        """从配置文件读取编译命令和日志命令"""
        try:
            if os.path.exists(self.config_file):
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    for line in f:
                        line = line.strip()
                        if line.startswith("build_command:"):
                            self.build_command = line.split(":", 1)[1].strip()
                        elif line.startswith("log_command:"):
                            self.log_command = line.split(":", 1)[1].strip()
                
                logger.info(f"配置文件加载成功: build_command={self.build_command}")
            else:
                logger.warning(f"配置文件不存在: {self.config_file}")
                # 创建默认配置文件
                with open(self.config_file, 'w', encoding='utf-8') as f:
                    f.write("# 编译命令配置\n")
                    f.write("# Windows下使用 && 连接多个命令\n")
                    f.write("build_command: cd win10_build && make -j4\n")
                logger.info(f"已创建默认配置文件: {self.config_file}")
                
        except Exception as e:
            logger.error(f"加载配置文件失败: {e}")
    
    def set_project_directory(self, directory: str) -> Dict[str, Any]:
        """设置项目目录"""
        try:
            if not os.path.exists(directory):
                return {"success": False, "error": f"目录不存在: {directory}"}
            
            if not os.path.isdir(directory):
                return {"success": False, "error": f"不是有效目录: {directory}"}
            
            self.project_dir = directory
            logger.info(f"项目目录已设置为: {directory}")
            
            return {"success": True, "message": f"项目目录已设置为: {directory}"}
        except Exception as e:
            logger.error(f"设置项目目录失败: {e}")
            return {"success": False, "error": str(e)}
    
    def _parse_progress_from_line(self, line: str) -> Optional[int]:
        """从日志行解析编译进度"""
        # 匹配 [ 66%] 或 [100%] 格式
        match = re.search(r'\[\s*(\d+)%\s*\]', line)
        if match:
            try:
                progress = int(match.group(1))
                return min(100, max(0, progress))
            except ValueError:
                pass
        return None
    
    def _update_progress(self, line: str):
        """更新编译进度"""
        progress = self._parse_progress_from_line(line)
        if progress is not None and progress > self.build_progress:
            self.build_progress = progress
            logger.info(f"编译进度: {progress}%")
    
    def _check_error_in_line(self, line: str) -> bool:
        """检查行中是否有错误"""
        error_patterns = [
            r'error:\s*.+',
            r'Error:\s*.+',
            r'ERROR:\s*.+',
            r'make\[\d+\]:\s*\*\*\*\s*.+',
            r'undefined reference to',
            r'No such file or directory',
            r'syntax error',
            r'fatal error:'
        ]
        
        for pattern in error_patterns:
            if re.search(pattern, line, re.IGNORECASE):
                self.last_error = line.strip()
                return True
        return False
    
    def start_build(self) -> Dict[str, Any]:
        """开始编译（同步等待完成）"""
        try:
            if self.is_building:
                return {"success": False, "error": "已有编译任务在进行中"}
            
            if not self.project_dir:
                return {"success": False, "error": "请先设置项目目录"}
            
            if not self.build_command:
                return {"success": False, "error": "编译命令未配置"}
            
            logger.info(f"开始编译: {self.build_command} in {self.project_dir}")
            
            # 重置状态
            self.is_building = True
            self.build_status = "building"
            self.build_progress = 0
            self.last_error = ""
            self.build_start_time = datetime.now()
            
            # 清空日志文件
            try:
                with open(self.log_file, 'w', encoding='utf-8') as f:
                    f.write(f"编译开始时间: {self.build_start_time.strftime('%Y-%m-%d %H:%M:%S')}\n")
                    f.write(f"项目目录: {self.project_dir}\n")
                    f.write(f"编译命令: {self.build_command}\n")
                    f.write("-" * 80 + "\n")
            except Exception as e:
                logger.error(f"清空日志文件失败: {e}")
            
            # 检查构建目录是否存在
            if "win10_build" in self.build_command:
                build_dir = os.path.join(self.project_dir, "win10_build")
                if not os.path.exists(build_dir):
                    error_msg = f"构建目录不存在: {build_dir}"
                    logger.error(error_msg)
                    with open(self.log_file, 'a', encoding='utf-8') as f:
                        f.write(f"错误: {error_msg}\n")
                    
                    self.is_building = False
                    self.build_status = "error"
                    self.last_error = error_msg
                    return {"success": False, "error": error_msg}
            
            # 执行编译命令
            try:
                # Windows下使用cmd /c执行命令
                if sys.platform == "win32":
                    cmd = ['cmd', '/c', self.build_command]
                    process = subprocess.Popen(
                        cmd,
                        cwd=self.project_dir,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        stdin=subprocess.DEVNULL,
                        text=True,
                        encoding='utf-8',
                        errors='replace',
                        bufsize=1,
                        creationflags=subprocess.CREATE_NO_WINDOW
                    )
                else:
                    # Linux/Mac使用bash
                    process = subprocess.Popen(
                        self.build_command,
                        cwd=self.project_dir,
                        shell=True,
                        executable='/bin/bash',
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        stdin=subprocess.DEVNULL,
                        text=True,
                        encoding='utf-8',
                        errors='replace',
                        bufsize=1
                    )
                
                # 实时读取输出
                while True:
                    line = process.stdout.readline()
                    if not line and process.poll() is not None:
                        break
                    
                    if line:
                        # 写入日志文件
                        with open(self.log_file, 'a', encoding='utf-8') as f:
                            f.write(line)
                        
                        # 更新进度
                        self._update_progress(line)
                        
                        # 检查错误
                        self._check_error_in_line(line)
                
                # 获取返回码
                return_code = process.wait()
                
            except Exception as e:
                error_msg = f"执行编译命令失败: {e}"
                logger.error(error_msg)
                with open(self.log_file, 'a', encoding='utf-8') as f:
                    f.write(f"执行错误: {error_msg}\n")
                
                self.is_building = False
                self.build_status = "error"
                self.last_error = error_msg
                return {"success": False, "error": error_msg}
            
            # 编译结束
            self.build_end_time = datetime.now()
            elapsed = self.build_end_time - self.build_start_time
            
            # 写入结束标记
            with open(self.log_file, 'a', encoding='utf-8') as f:
                f.write("-" * 80 + "\n")
                f.write(f"编译结束时间: {self.build_end_time.strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"编译耗时: {elapsed}\n")
                f.write(f"编译状态: {'成功' if return_code == 0 else '失败'}\n")
                f.write(f"返回码: {return_code}\n")
                f.write(f"编译进度: {self.build_progress}%\n")
            
            # 更新状态
            self.is_building = False
            if return_code == 0:
                self.build_status = "success"
                self.build_progress = 100
                logger.info(f"编译成功完成，耗时: {elapsed}")
            else:
                self.build_status = "error"
                logger.error(f"编译失败，返回码: {return_code}，耗时: {elapsed}")
            
            # 返回结果
            if return_code == 0:
                return {
                    "success": True,
                    "message": f"编译成功完成！耗时: {elapsed}",
                    "elapsed_time": str(elapsed),
                    "progress": self.build_progress,
                    "log_file": self.log_file
                }
            else:
                return {
                    "success": False,
                    "error": f"编译失败，返回码: {return_code}",
                    "last_error": self.last_error,
                    "progress": self.build_progress,
                    "elapsed_time": str(elapsed),
                    "log_file": self.log_file
                }
            
        except Exception as e:
            logger.error(f"启动编译失败: {e}")
            self.is_building = False
            self.build_status = "error"
            return {"success": False, "error": str(e)}
    
    def get_build_progress(self) -> Dict[str, Any]:
        """获取编译进度"""
        return {
            "is_building": self.is_building,
            "status": self.build_status,
            "progress": self.build_progress,
            "last_error": self.last_error,
            "project_dir": self.project_dir,
            "start_time": self.build_start_time.strftime('%Y-%m-%d %H:%M:%S') if self.build_start_time else None,
            "end_time": self.build_end_time.strftime('%Y-%m-%d %H:%M:%S') if self.build_end_time else None
        }
    
    def get_build_log(self) -> Dict[str, Any]:
        """获取编译日志"""
        try:
            if not os.path.exists(self.log_file):
                return {"success": False, "error": "日志文件不存在"}
            
            with open(self.log_file, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
            
            file_size = os.path.getsize(self.log_file)
            file_mtime = datetime.fromtimestamp(os.path.getmtime(self.log_file)).strftime('%Y-%m-%d %H:%M:%S')
            
            return {
                "success": True,
                "content": content,
                "file_size": file_size,
                "file_mtime": file_mtime,
                "file_path": self.log_file,
                "line_count": len(content.splitlines())
            }
            
        except Exception as e:
            logger.error(f"读取日志文件失败: {e}")
            return {"success": False, "error": str(e)}

# 全局编译管理器实例
build_manager = BuildManager()

async def handle_list_tools() -> List[Dict[str, Any]]:
    """处理工具列表请求"""
    return [
        {
            "name": "set_project_directory",
            "description": "设置项目目录",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "directory": {
                        "type": "string",
                        "description": "项目目录路径"
                    }
                },
                "required": ["directory"]
            }
        },
        {
            "name": "start_build",
            "description": "开始编译项目（同步等待完成）",
            "inputSchema": {
                "type": "object",
                "properties": {},
                "required": []
            }
        },
        {
            "name": "get_build_progress",
            "description": "获取编译进度",
            "inputSchema": {
                "type": "object",
                "properties": {},
                "required": []
            }
        },
        {
            "name": "get_build_log",
            "description": "获取编译日志",
            "inputSchema": {
                "type": "object",
                "properties": {},
                "required": []
            }
        },
        {
            "name": "reload_config",
            "description": "重新加载配置文件",
            "inputSchema": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    ]

async def handle_call_tool(name: str, arguments: Dict[str, Any]) -> List[Dict[str, Any]]:
    """处理工具调用"""
    global build_manager
    
    try:
        if name == "set_project_directory":
            result = build_manager.set_project_directory(arguments["directory"])
            if result["success"]:
                return [{"type": "text", "text": result["message"]}]
            else:
                return [{"type": "text", "text": f"设置项目目录失败: {result['error']}"}]
        
        elif name == "start_build":
            # 同步执行编译（会等待编译完成）
            result = build_manager.start_build()
            
            if result["success"]:
                message = f"✅ 编译成功！\n"
                message += f"耗时: {result['elapsed_time']}\n"
                message += f"进度: {result['progress']}%\n"
                message += f"日志文件: {result['log_file']}\n"
                return [{"type": "text", "text": message}]
            else:
                message = f"❌ 编译失败！\n"
                message += f"错误: {result['error']}\n"
                if 'last_error' in result and result['last_error']:
                    message += f"具体错误: {result['last_error']}\n"
                message += f"进度: {result.get('progress', 0)}%\n"
                message += f"耗时: {result.get('elapsed_time', '未知')}\n"
                message += f"日志文件: {result.get('log_file', '未知')}\n"
                return [{"type": "text", "text": message}]
        
        elif name == "get_build_progress":
            result = build_manager.get_build_progress()
            
            if result["is_building"]:
                status_text = "编译中"
            elif result["status"] == "success":
                status_text = "编译成功"
            elif result["status"] == "error":
                status_text = "编译失败"
            else:
                status_text = "未开始编译"
            
            message = f"编译状态: {status_text}\n"
            message += f"编译进度: {result['progress']}%\n"
            message += f"项目目录: {result['project_dir'] or '未设置'}\n"
            
            if result["start_time"]:
                message += f"开始时间: {result['start_time']}\n"
            
            if result["end_time"]:
                message += f"结束时间: {result['end_time']}\n"
            
            if result["last_error"]:
                message += f"最后错误: {result['last_error']}\n"
            
            return [{"type": "text", "text": message}]
        
        elif name == "get_build_log":
            result = build_manager.get_build_log()
            if result["success"]:
                # 添加文件信息
                info = f"📋 编译日志信息\n"
                info += "=" * 50 + "\n"
                info += f"文件: {result['file_path']}\n"
                info += f"大小: {result['file_size']:,} 字节\n"
                info += f"最后修改: {result['file_mtime']}\n"
                info += f"行数: {result['line_count']}\n"
                info += "=" * 50 + "\n\n"
                
                content = info + result["content"]
                return [{"type": "text", "text": content}]
            else:
                return [{"type": "text", "text": f"获取日志失败: {result['error']}"}]
        
        elif name == "reload_config":
            build_manager._load_config()
            return [{"type": "text", "text": "配置文件已重新加载"}]
        
        else:
            return [{"type": "text", "text": f"未知工具: {name}"}]
            
    except Exception as e:
        logger.error(f"工具调用错误: {e}")
        return [{"type": "text", "text": f"工具调用失败: {str(e)}"}]

async def handle_list_resources() -> List[Dict[str, Any]]:
    """处理资源列表请求"""
    return [
        {
            "uri": "build://status",
            "name": "编译状态",
            "description": "当前编译状态和进度",
            "mimeType": "application/json"
        },
        {
            "uri": "build://config",
            "name": "编译配置",
            "description": "当前编译配置信息",
            "mimeType": "application/json"
        }
    ]

async def handle_read_resource(uri: str) -> str:
    """处理读取资源请求"""
    global build_manager
    
    if uri == "build://status":
        status = build_manager.get_build_progress()
        return json.dumps(status, ensure_ascii=False, indent=2)
    
    elif uri == "build://config":
        config = {
            "project_dir": build_manager.project_dir,
            "build_command": build_manager.build_command,
            "log_command": build_manager.log_command,
            "log_file": build_manager.log_file,
            "config_file": build_manager.config_file,
            "is_building": build_manager.is_building
        }
        return json.dumps(config, ensure_ascii=False, indent=2)
    
    else:
        return json.dumps({"error": f"未知资源: {uri}"}, ensure_ascii=False, indent=2)

async def main():
    """主函数"""
    logger.info("启动项目编译 MCP 服务器 - 简化版...")
    
    try:
        # 初始化编译管理器
        global build_manager
        logger.info(f"配置文件位置: {build_manager.config_file}")
        logger.info(f"日志文件位置: {build_manager.log_file}")
        
        # 主循环
        while True:
            # 读取一行输入
            line = await asyncio.get_event_loop().run_in_executor(None, sys.stdin.readline)
            if not line:
                logger.info("没有输入，退出")
                break
                
            line = line.strip()
            if not line:
                continue
                
            logger.debug(f"收到输入: {line}")
            
            try:
                # 解析 JSON-RPC 消息
                message = json.loads(line)
                message_id = message.get("id", 0)
                
                # 处理初始化请求
                if message.get("method") == "initialize":
                    response = {
                        "jsonrpc": "2.0",
                        "id": message_id,
                        "result": {
                            "protocolVersion": "2024-11-05",
                            "capabilities": {
                                "tools": {},
                                "resources": {}
                            },
                            "serverInfo": {
                                "name": "build-mcp-server",
                                "version": "1.0.0"
                            }
                        }
                    }
                    print(json.dumps(response), flush=True)
                    logger.info("发送初始化响应")
                
                # 处理工具列表请求
                elif message.get("method") == "tools/list":
                    tools = await handle_list_tools()
                    response = {
                        "jsonrpc": "2.0", 
                        "id": message_id,
                        "result": {
                            "tools": tools
                        }
                    }
                    print(json.dumps(response), flush=True)
                    logger.info("发送工具列表响应")
                
                # 处理工具调用请求
                elif message.get("method") == "tools/call":
                    name = message["params"]["name"]
                    arguments = message["params"].get("arguments", {})
                    
                    result = await handle_call_tool(name, arguments)
                    response = {
                        "jsonrpc": "2.0",
                        "id": message_id,
                        "result": {
                            "content": result
                        }
                    }
                    print(json.dumps(response), flush=True)
                    logger.info(f"发送工具调用响应: {name}")
                
                # 处理资源列表请求
                elif message.get("method") == "resources/list":
                    resources = await handle_list_resources()
                    response = {
                        "jsonrpc": "2.0",
                        "id": message_id,
                        "result": {
                            "resources": resources
                        }
                    }
                    print(json.dumps(response), flush=True)
                    logger.info("发送资源列表响应")
                
                # 处理读取资源请求
                elif message.get("method") == "resources/read":
                    uri = message["params"]["uri"]
                    result = await handle_read_resource(uri)
                    response = {
                        "jsonrpc": "2.0",
                        "id": message_id,
                        "result": {
                            "contents": [
                                {
                                    "uri": uri,
                                    "mimeType": "application/json",
                                    "text": result
                                }
                            ]
                        }
                    }
                    print(json.dumps(response), flush=True)
                    logger.info(f"发送读取资源响应: {uri}")
                
                # 处理未知方法
                else:
                    response = {
                        "jsonrpc": "2.0",
                        "id": message_id,
                        "error": {
                            "code": -32601,
                            "message": f"Method not found: {message.get('method')}"
                        }
                    }
                    print(json.dumps(response), flush=True)
                    logger.warning(f"未知方法: {message.get('method')}")
                    
            except json.JSONDecodeError as e:
                logger.error(f"JSON 解析错误: {e}")
                error_response = {
                    "jsonrpc": "2.0",
                    "id": 0,
                    "error": {
                        "code": -32700,
                        "message": "Parse error"
                    }
                }
                print(json.dumps(error_response), flush=True)
            except Exception as e:
                logger.error(f"处理消息时发生错误: {e}")
                error_response = {
                    "jsonrpc": "2.0",
                    "id": message.get("id", 0),
                    "error": {
                        "code": -32603,
                        "message": f"Internal error: {str(e)}"
                    }
                }
                print(json.dumps(error_response), flush=True)
                
    except Exception as e:
        logger.error(f"服务器错误: {e}")
        error_response = {
            "jsonrpc": "2.0",
            "id": None,
            "error": {
                "code": -32603,
                "message": f"Internal error: {str(e)}"
            }
        }
        print(json.dumps(error_response), flush=True)
        raise
    
    logger.info("项目编译 MCP 服务器退出")

if __name__ == "__main__":
    # 确保输出是行缓冲的
    if sys.stdout:
        sys.stdout.reconfigure(line_buffering=True)
    
    # 运行服务器
    asyncio.run(main())