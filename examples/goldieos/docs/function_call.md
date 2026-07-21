# ConvAI Examples Function Call 最佳实践

本文档基于华为 ConvAI SDK，说明如何在 Examples 中实现 Function Call 功能。以**设置闹钟**为例。

---

## 目录

1. [架构概述](#架构概述)
2. [快速开始：新增一个 Function Call](#快速开始新增一个-function-call)
3. [Handler 函数签名](#handler-函数签名)
4. [回复机制](#回复机制)
5. [注册回调](#注册回调)
6. [设置闹钟完整示例](#设置闹钟完整示例)
7. [协议参考](#协议参考)

---

## 架构概述

所有来自 AI 的 Function Call 在 `apps/settings/main_app.cpp` 的 `cloud_message_callback` 中统一处理。

```
Cloud AI Server
    │  WebSocket
    ▼
ConvAI SDK → convai_bridge → cloud_message_callback()
                                 │
                                 ├─ 解析 type / calls[]
                                 ├─ 查表 func_call_registry[]
                                 ├─ 调用匹配的 handler
                                 └─ 自动构建回复 function_call_output
```

---

## 快速开始：新增一个 Function Call

### 第一步：编写 handler 函数

```c
static bool handle_xxx(const char *call_id, cJSON *args_json,
                        char *output_buf, size_t buf_size,
                        const char **output_str)
{
    // 1. 解析必选参数
    cJSON *param = cJSON_GetObjectItem(args_json, "param_name");
    if (!param || !cJSON_IsString(param)) {
        *output_str = "{\"result\":\"error\",\"message\":\"缺少xxx参数\"}";
        return true;
    }

    // 2. 执行业务逻辑（调用设备服务等）

    // 3. 自定义回复（可选，不设置则使用默认回复）
    snprintf(output_buf, buf_size,
             "{\"result\":\"success\",\"message\":\"操作完成\"}");
    *output_str = output_buf;

    return true;
}
```

### 第二步：注册到分发表

```c
static const struct {
    const char      *name;
    FuncCallHandler  handler;
} func_call_registry[] = {
    { "set_alarm",   handle_set_alarm },
    { "get_weather", handle_get_weather },
    { "xxx",         handle_xxx },          // ← 新增这一行
};
```

两步完成，`cloud_message_callback` 中的主循环会自动完成 JSON 解析、遍历 calls、分派 handler、构建回复。

---

## Handler 函数签名

```c
typedef bool (*FuncCallHandler)(
    const char *call_id,      // function call 的唯一 ID
    cJSON *args_json,         // arguments 已解析为 cJSON 对象
    char *output_buf,         // 256 字节栈缓冲区，供 snprintf 使用
    size_t buf_size,          // output_buf 大小
    const char **output_str   // 指向回复 JSON 字符串的指针
);
```

| 参数 | 说明 |
|------|------|
| `call_id` | Function Call 唯一 ID（一般无需使用，主循环自动对应） |
| `args_json` | `arguments` 字段经 `cJSON_Parse` 解析后的对象 |
| `output_buf` | 256 字节栈缓冲区 |
| `buf_size` | 缓冲区大小 |
| `output_str` | 输出指针，指向回复的 JSON 字符串 |

**返回值**：`true` 表示已处理。

---

## 回复机制

### 默认回复

handler 不修改 `*output_str` 时，主循环自动使用默认回复：

```json
{"result": "success", "message": "成功"}
```

### 自定义回复

```c
// 方式一：静态字符串（无动态数据时推荐，零开销）
*output_str = "{\"result\":\"error\",\"message\":\"参数无效\"}";

// 方式二：snprintf 动态构造（需要嵌入变量时使用）
snprintf(output_buf, buf_size,
         "{\"result\":\"success\",\"message\":\"已设置\",\"index\":%d}", index);
*output_str = output_buf;
```

> **注意**：`output_buf` 大小为 256 字节。snprintf 不会溢出，但超长内容会被截断。

### 主循环自动构建

handler 只需设置 `*output_str`，主循环自动构建完整的回复 JSON：

```json
{
  "type": "conversation.items.create",
  "items": [
    {
      "type": "function_call_output",
      "call_id": "<自动填入>",
      "output": "<*output_str 指向的内容>"
    }
  ]
}
```

---

## 注册回调

在应用初始化时，通过 `convai_bridge_on_message` 注册回调：

```c
convai_bridge_on_message(cloud_message_callback);
```

---

## 设置闹钟完整示例

### 场景

用户说「帮我设置下午4点的开会闹钟」，Agent 下发 `set_alarm`：

```json
{
  "type": "response.function_call_arguments.done",
  "calls": [
    {
      "call_id": "call_alarm_001",
      "name": "set_alarm",
      "arguments": "{\"time\":\"16:00\",\"label\":\"开会\",\"repeat\":\"weekdays\"}"
    }
  ]
}
```

### 参数说明

| 参数 | 类型 | 说明 |
|------|------|------|
| `time` | string | 闹钟时间 `HH:MM` |
| `label` | string | 闹钟标签 |
| `repeat` | string | `none`（一次性）、`daily`（每天）、`weekdays`（工作日） |
| `weekdays` | bool[] | 可选，按星期几设置的布尔数组 |
| `enabled` | bool | 可选，是否启用，默认 `true` |

### Handler 实现

```c
static bool handle_set_alarm(const char *call_id, cJSON *args_json,
                              char *output_buf, size_t buf_size,
                              const char **output_str)
{
    (void)call_id;

    /* ---- 解析 time 字段："HH:MM" ---- */
    int hour = 0, minute = 0;
    cJSON *time_item = cJSON_GetObjectItem(args_json, "time");
    if (time_item && cJSON_IsString(time_item)) {
        const char *time_str = time_item->valuestring;
        if (strlen(time_str) == 5 && time_str[2] == ':') {
            hour   = (time_str[0] - '0') * 10 + (time_str[1] - '0');
            minute = (time_str[3] - '0') * 10 + (time_str[4] - '0');
        }
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        *output_str = "{\"result\":\"error\",\"message\":\"时间格式无效\"}";
        return true;
    }

    /* ---- 获取闹钟服务 ---- */
    AlarmService *alarm_svc = (AlarmService*)get_service(ALARM_SERVICE_INDEX);
    if (!alarm_svc) {
        *output_str = "{\"result\":\"error\",\"message\":\"闹钟服务不可用\"}";
        return true;
    }

    /* ---- 构造闹钟对象 ---- */
    AlarmInfo alarm;
    memset(&alarm, 0, sizeof(AlarmInfo));
    alarm.m_hour     = (char)hour;
    alarm.m_min      = (char)minute;
    alarm.enabled    = true;
    alarm.ring_index = 0;

    /* ---- 解析 repeat ---- */
    cJSON *repeat_item = cJSON_GetObjectItem(args_json, "repeat");
    if (repeat_item && cJSON_IsString(repeat_item)) {
        const char *repeat = repeat_item->valuestring;
        if (strcmp(repeat, "daily") == 0)
            for (int w = 0; w < 7; w++) alarm.weekdays[w] = true;
        else if (strcmp(repeat, "weekdays") == 0)
            for (int w = 0; w < 5; w++) alarm.weekdays[w] = true;
    } else {
        for (int w = 0; w < 7; w++) alarm.weekdays[w] = true;
    }

    /* ---- 添加闹钟 ---- */
    int ret = alarm_svc->add_alarm(&alarm);
    if (ret >= 0) {
        snprintf(output_buf, buf_size,
                 "{\"result\":\"success\",\"message\":\"闹钟已设置\",\"index\":%d}", ret);
        *output_str = output_buf;
    } else if (ret == -2) {
        *output_str = "{\"result\":\"error\",\"message\":\"闹钟已满,最多10个\"}";
    } else {
        *output_str = "{\"result\":\"error\",\"message\":\"添加失败\"}";
    }

    return true;
}
```

### 注册到分发表

```c
static const struct {
    const char      *name;
    FuncCallHandler  handler;
} func_call_registry[] = {
    { "set_alarm",   handle_set_alarm },
    { "get_weather", handle_get_weather },
};
```

---

## 协议参考

### 消息流向

| 消息 | 方向 | 说明 |
|------|------|------|
| `response.function_call_arguments.done` | Agent → Examples | 携带调用参数 |
| `conversation.items.create` | Examples → Agent | 返回执行结果（主循环自动构建） |

### AI 下发的 JSON

```json
{
  "type": "response.function_call_arguments.done",
  "calls": [
    {
      "call_id": "call_xxx",
      "name": "set_alarm",
      "arguments": "{\"time\":\"16:00\",\"label\":\"开会\",\"repeat\":\"none\"}"
    }
  ]
}
```

注意：`arguments` 是 JSON 字符串（二次编码），由主循环调用 `cJSON_Parse` 解析后传给 handler。

### 回复 JSON（主循环自动构建）

```json
{
  "type": "conversation.items.create",
  "items": [
    {
      "type": "function_call_output",
      "call_id": "call_xxx",
      "output": "<handler 设置的 *output_str>"
    }
  ]
}
```

### 已注册的 Function Calls

| name | 功能 | 参数 |
|------|------|------|
| `set_alarm` | 设置闹钟 | `{"time":"16:00","label":"开会","repeat":"weekdays"}` |
| `get_weather` | 查询天气 | `{"location":"深圳"}` |

---

> **版本信息：** 本文档基于华为 ConvAI SDK 26.6.0 版本编写。
