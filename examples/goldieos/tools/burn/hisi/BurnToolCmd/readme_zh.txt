欢迎使用BurnToolCmd命令行工具!
此工具功能主要是：烧写镜像到Flash上、擦除Flash和从板端读取数据写入文件。

1.烧写镜像功能说明
步骤如下：
(1)配置config文件夹上的burn.config(配置文件中#表示该行注释)，烧写前必须配置，配置项有：
chipName               芯片名，如 suqin
filePath               分区表文件路径，支持文件格式：.bin\.fwpkg\.xml
comport                串口号
serialCommandTimeout   命令超时时间
serverip               服务器IP
ipaddr                 板端IP
netmask                板端子网掩码
gatewayip              板端网关
udpport                udp烧写使用端口号
ethaddr                MAC地址（默认工具会自动随机配置，如需手动设置，需要删掉ethaddr配置项前面的#）
portConfigurable       网口通信是否可以配置端口
port                   网口使用的端口号
tftpTimeout            等待网口返回数据的超时时间
isSocket               是否启用串口服务器
serialServerIp         串口服务器的ip
serialServerPort       串口服务器的端口号
mode                   传输方式
usbToEthernet          是否使用usb转网口的传输方式
usbSerialNumber        串口+usb传输方式并发烧写，每一个usb的标识
usbDeviceNumber        USBBootrom传输方式并发烧写，每个usb的id
frequency              jtag烧写的频率
debugBoard        jtag小板的型号
isBurnProgrammer       是否烧写programmar文件
programmer             programmer文件的路径
isSplit                配置为true时烧写拆分过的zip镜像

(3)在命令行下，进入burntoolcli所在目录；
(4)输入命令按分区烧写：BurnToolCmd --burn（linux）
                       BurnToolCmd.exe --burn（windows）
   如果你不想配置config文件，可以输入以下命令烧写.
                      <chipName>         <mode>       <serial port>  -speed  [speed]  -stopBit  [stop bits] -parity  [parity]    <serverIp>       <ipaddr>       <netMask>    <gatewayIp>    -tftpPort [tftpPort]  <debugBoard> -frequency  [frequency]   -address [address]      <filePath>         [programmer]            -re [reboot] -c [burnCheck]
BurnToolCmd --burn  -n WUDANGSTICK  -m   serial         COM0                  115200                1                   0                                                                                                                                                            -f  filePath       -p  programmerPath   -re     0    -c    0
BurnToolCmd --burn  -n WUDANGSTICK  -m   net            COM0                  115200                1                   0         xxx.xxx.xx.xx  xxx.xxx.xx.x    xxx.xxx.xxx.x  xxx.xxx.xx.x               69                                                                        -f  filePath       -p  programmerPath   -re     0    -c    0
BurnToolCmd --burn  -n WUDANGSTICK  -m   USBBootrom                                                                                                                                                                                                                                  -f  filePath       -p  programmerPath   -re     0    -c    0
BurnToolCmd --burn  -n WUDANGSTICK  -m   USBAndSerial   COM0                  115200                1                   0                                                                                                                                                            -f  filePath       -p  programmerPath   -re     0    -c    0
BurnToolCmd --burn  -n WUDANGSTICK  -m   jtag                                                                                                                                                                             0                        4M                     0x3000000  -f  filePath       -p  programmerPath   -re     0    -c    0
BurnToolCmd --burn  -n WUDANGSTICK  -m   swd                                                                                                                                                                              0                        4M                     0x3000000  -f  filePath       -p  programmerPath   -re     0    -c    0
BurnToolCmd --burn  -n WUDANGSTICK  -m   JTAGAndNet                                                                              xxx.xxx.xx.xx  xxx.xxx.xx.x    xxx.xxx.xxx.x  xxx.xxx.xx.x                69             0                        4M                                -f  filePath       -p  programmerPath   -re     0    -c    0
BurnToolCmd --burn  -n WUDANGSTICK  -m   JTAGAndSerial  COM0                  115200                1                   0                                                                                                 0                        4M                                -f  filePath       -p  programmerPath   -re     0    -c    0
mode:
serial
net
usb
USBAndSerial
JTAG
JTAGAndNet
JTAGAndSerial
speed: 波特率可选项有：1200、2400、4800、9600、19200、38400、57600、115200、230400、460800、921600
stop bit: 停止位可选项有：0(1位停止位)、1(1.5位停止位)、2(2位停止位)
parity: 奇偶校验可选项有：0(无校验)、1(奇校验)、2(偶校验)
debugBoard: Hispark-Trace(0)、 Hispark-Link(1)
reboot: 烧录完成后是否重启，可选项：0（不重启）、1（重启）
burnCheck：烧录完成后是否回读校验，可选项：0（不校验）、1（校验）

注：如果需要指定分区表文件路径，请增加-f filePath,如BurnToolCmd --burn -f filePath， 文件格式支持: .xml/.bin/.fwpkg
    如果需要指定programmer镜像，请增加-p programmer,如BurnToolCmd --burn -p programmerPath
    如果需要指定burn.config配置文件，请增加-c burnConfigPath,如BurnToolCmd --burn -c burnConfigPath
    如果需要指定串口服务器的账号和密码，请增加 -u  userName passWord，如，BurnToolCmd --burn -u  userName passWord
    如果需要制定串口服务器的ip和端口，请增加 --serialServerIp 串口服务器的ip --serialServerPort 串口服务器的端口号
(5)出现如下打印：SerialPort has been connented, Please power off, then power on the device.If it doesn't work, please try to repower on. 请手动上电，开始烧写。


2.擦除功能说明
BurnToolCmd --erase     <chipName>      <mode>     <port>  -speed   [speed]  -stopBit  [stop bits] -parity  [parity]   -debugBoard <debugBoard>  -frequency [frequency]  -address [address]  -f  <filePath>          [programmer]       [configPath]           -e [eraseSize]
BurnToolCmd --erase  -n WUDANGSTICK  -m serial      COM0             115200                 1                  0                                                                                       -f   filePath     -p   programmerPath  -c   configPath  -e    1000
BurnToolCmd --erase  -n WUDANGSTICK  -m usb                                                                                                                                                            -f   filePath     -p   programmerPath  -c   configPath  -e    1000
BurnToolCmd --erase  -n WUDANGSTICK  -m jtag                                                                                                   0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath  -e    1000
BurnToolCmd --erase  -n WUDANGSTICK  -m swd                                                                                                    0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath  -e    1000

3.读取文件功能说明
BurnToolCmd --read     <chipName>      <mode>     <port>  -speed   [speed]  -stopBit  [stop bits] -parity  [parity]   -debugBoard <debugBoard>  -frequency [frequency]  -address [address]  -f  <filePath>          [programmer]       [configPath]             -r [readSize]
BurnToolCmd --read  -n WUDANGSTICK  -m serial      COM0             115200                 1                  0                                                                                       -f   filePath     -p   programmerPath  -c   configPath    -r   1000
BurnToolCmd --read  -n WUDANGSTICK  -m usb                                                                                                                                                            -f   filePath     -p   programmerPath  -c   configPath    -r   1000
BurnToolCmd --read  -n WUDANGSTICK  -m jtag                                                                                                   0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath    -r   1000
BurnToolCmd --read  -n WUDANGSTICK  -m swd                                                                                                    0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath    -r   1000


4.拆分镜像功能说明
BurnToolCmd --splitimg   [xmlPath]  [ddrSize]  [outputPath]
xmlPath：分区表路径
ddrSize：ddr大小（M），拆分镜像大小为ddrSizeSize的一半
outputPath：拆分文件及分区表目录，可选配置，默认xmlPath目录下

5.usb并发烧写功能说明
步骤如下：
(1)配置config文件夹上的burn.config，将usbDeviceNumber设为0，然后使用usb烧写；
(2)工具会打印出usb号，接下来将usbDeviceNumber设置成打印出的usb号：
(3)使用多个工具，每个工具的usbDeviceNumber不同，就可以实现usb并发烧写功能：

[设置]
emmcWriteSpeed         mmc write命令执行速率,默认为3072
emmcDiskFormat         擦除时是否使用深度格式化，默认为false
eraseBeforeBurn        是否烧写前进行擦除，默认为false
resetBeforeBurn        是否烧写前进行软复位，默认为false
openDebug              是否开启debug打印，默认为false
splitFileSize          拆分文件大小，默认32，单位M

6.Touch烧写功能说明
-n            		   芯片名: hi3286
-m               	   烧写方式: usb
-usbTransformsType     usb传输方式：UsbToSpi
-touch                 touchbin文件路径
-image                 imagebin文件路径
-vctm                  vctmbin文件路径
注意： touchbin、imagebin和vctmbin可以只输入其中一个文件进行烧写、或者是输入两个文件、亦或者输入三个文件都进行烧写

命令示例:
BurnToolCmd.exe --burn -n hi3286 -m usb --usbTransformsType UsbToSpi --touch E:/xcc/image_old/touch.bin --image  E:/xcc/image_old/image.bin --vctm E:/xcc/image_old/vctm.bin

7.ws63烧写、擦除命令说明：
公共参数说明：
-n                     芯片名: ws63
-m                     烧写方式: serial COM10
--baudRate             串口波特率：115200

烧写命令示例：
-f                     烧写文件
BurnToolCmd.exe --burn -n ws63 -m serial COM10 --baudRate 115200  -f E:/ws63-liteos-app_all.fwpkg

擦除命令示例：
-l                     loader文件
BurnToolCmd.exe --erase -n ws63 -m serial COM10 --baudRate 115200  -l E:/ws63-liteos-app_all.fwpkg

上载命令示例：
-l                     loader文件
-f                     upload文件
BurnToolCmd.exe --read -n ws63 -m serial COM10 -a 0x200000 -r 0x780  -l E:/ws63-liteos-app_all.fwpkg  -f upload.bin
FQA：
烧写失败返回错误码说明：
错误码 1：烧写的命令参数错误
错误码 2：导入文件失败，例如导入config文件，xml文件，program文件失败
错误码 3：打开串口/网口/USB/JTAG设备失败
错误码 4：烧写fastboot失败
错误码 5：烧写分区文件失败
错误码 6：擦除失败
错误码 7：从板端读取数据到文件失败
错误码 8：空指针异常
错误码 9: new或者malloc失败
错误码 10: 发送命令失败
