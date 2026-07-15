Welcome to use the BurnToolCmd command line tool!
The main functions of this tool are: burn and write the image onto the Flash, erase the Flash, and read the data from the board and write the file.

1. Description About the Function of Burning Images
The procedures are as follows:
(1) Configure burn.config in the config folder before burning images (# in this configuration file indicates the comments). The configuration items include:
chipName               Chip name, such as suqin
filePath               Partition table file path, supported file format: .bin\.fwpkg\.xml
xmlPathDefault         Indicates whether the image path is the path where the XML file is stored.
comport                Number of the serial port
serverip               IP address of the server
ipaddr                 IP address of the board
netmask                Subnet mask of the board
gatewayip              Board gateway
udpport                UDP burning port
ethaddr                MAC address (The tool automatically configures a random MAC address by default. If you want to manually configure the MAC address, delete the # before the ethaddr configuration item.)
typeFrameFrequency     Start frame communication time with the board
portConfigurable       Can a port be configured for network port communication
port                   Port number used by the network port
tftpTimeout            Timeout interval for waiting for data returned by the network port
isSocket               Whether to enable the serial port server
serialServerIp         IP address of the serial port server
serialServerPort       Port number of the serial port server
mode                   Tranmission mode
usbToEthernet          whether to use the USB-to-network port transmission mode
usbSerialNumber        USBAndSerial transfer mode concurrency write, each USB identifier
usbDeviceNumber        USBBootrom transfer mode concurrency write, each USB ID
frequency              Jtag burning frequency
debugBoard        Model of the Jtag sub-board
baudRate               I2C burning frequency
isBurnProgrammer       Whether to burn the programmar file
programmer             Path to the programmer file
isSplit                Set isSplit to true and burn the split .zip image

(3) Enter the directory where burntoolcli is located at the command line.
(4) Enter the (burntoolcli --burn) command to burn images according to the partitioning file.The JRE used by the Linux OS is recommended.
              BurnToolCmd --burn（linux） 
              BurnToolCmd.exe --burn（windows）
   If you do not want to configure the config file, run the following command to burn the file:
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
speed: Baud rate options are：1200、2400、4800、9600、19200、38400、57600、115200、230400、460800、921600
stop bit: Stop bit options are：0(1 stop bit)、1(1.5 stop bit)、2(2 stop bit)
parity: Parity check options are：0(no parity)、1(Odd parity)、(even parity)
debugBoard: Hispark-Trace(0)、 Hispark-Link(1)
reboot: Indicates whether to restart the system after the programming is complete. The options are as follows: 0 (not restart) 1 (restart)
burnCheck：Indicates whether to check the readback after programming. The options are as follows: 0 (no check) and 1 (check).

Note: If you need to specify the partition table xmlPath path, add -f filePath, such as BurnToolCmd --burn -f filePath, File format support: .xml/.bin/.fwpkg
      If you need to specify the programmer image, please add -p programmer, such as BurnToolCmd --burn -p programmerPath
      If you need to specify the burn.config, please add -c burnConfigPath, such as BurnToolCmd --burn -c burnConfigPath
      If you need to specify the account and password for logging in to the serial port server, please add -u userName passWord， such as，BurnToolCmd --burn -u  userName passWord
(5) The following information is displayed: SerialPort has been connected. Please power off, then power on the device. If it doesn't work, please try to power on again. Then manually power on the tool and start burning.


2.Erasing Function Description
BurnToolCmd --erase     <chipName>      <mode>     <port>  -speed   [speed]  -stopBit  [stop bits] -parity  [parity]   -debugBoard <debugBoard>  -frequency [frequency]  -address [address]  -f  <filePath>          [programmer]       [configPath]           -e [eraseSize]
BurnToolCmd --erase  -n WUDANGSTICK  -m serial      COM0             115200                 1                  0                                                                                       -f   filePath     -p   programmerPath  -c   configPath  -e    1000
BurnToolCmd --erase  -n WUDANGSTICK  -m usb                                                                                                                                                            -f   filePath     -p   programmerPath  -c   configPath  -e    1000
BurnToolCmd --erase  -n WUDANGSTICK  -m jtag                                                                                                   0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath  -e    1000
BurnToolCmd --erase  -n WUDANGSTICK  -m swd                                                                                                    0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath  -e    1000

3.Read file function description
BurnToolCmd --read     <chipName>      <mode>     <port>  -speed   [speed]  -stopBit  [stop bits] -parity  [parity]   -debugBoard <debugBoard>  -frequency [frequency]  -address [address]  -f  <filePath>          [programmer]       [configPath]            -r [readSize]
BurnToolCmd --read  -n WUDANGSTICK  -m serial      COM0             115200                 1                  0                                                                                       -f   filePath     -p   programmerPath  -c   configPath   -r   1000
BurnToolCmd --read  -n WUDANGSTICK  -m usb                                                                                                                                                            -f   filePath     -p   programmerPath  -c   configPath   -r   1000
BurnToolCmd --read  -n WUDANGSTICK  -m jtag                                                                                                   0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath   -r   1000
BurnToolCmd --read  -n WUDANGSTICK  -m swd                                                                                                    0                            4M              0x3000000  -f   filePath     -p   programmerPath  -c   configPath   -r   1000

4. Splitting an image
BurnToolCmd --splitimg [xmlPath] [ddrSize] [outputPath]
xmlPath: partition table path.
ddrSize: DDR size (MB)，the split image size is half of ddrSizeSize.
outputPath: directory for storing split files and partition tables. Optional configuration. The default directory is xmlPath.

5. USB Concurrency Function Description
The procedure is as follows:
(1) Configure the burn.config file in the config folder, set usbDeviceNumber to 0, and use the USB to burn the file.
(2) The tool prints the USB number. Then, set usbDeviceNumber to the printed USB number.
(3) Use multiple tools, each with a different usbDeviceNumber, to implement the USB write function.

[setting]
emmcWriteSpeed         Execution rate of the mmc write command. The default value is 3072.
emmcDiskFormat         Whether to implement in-depth formatting during the erase operation. The default value is false.
eraseBeforeBurn        Whether to perform the erase operation before burning. The default value is false.
resetBeforeBurn        Whether to perform soft reset before programming, the default is false.
openDebug              Whether to enable debug printing, the default is false.
splitFileSize          Split file size, default 32, unit M

6.Touch chip burn command description：
-n            		   chipName: hi3286
-m               	   mode: usb
-usbTransformsType     usb transforms type ：UsbToSpi
-touch                 touchbin file path
-image                 imagebin file path
-vctm                  vctmbin file path

Example of Burning Commands:
BurnToolCmd.exe --burn -n hi3286 -m usb --usbTransformsType UsbToSpi --touch E:/xcc/image_old/touch.bin --image  E:/xcc/image_old/image.bin --vctm E:/xcc/image_old/vctm.bin

7.ws63 chip burn、erase command description：
Common parameters：
-n                     chipName: ws63
-m                     mode: serial COM10
--baudRate             serial baudRate：115200

Example of Burning Commands：
-f                     burn file
BurnToolCmd.exe --burn -n ws63 -m serial COM10 --baudRate 115200  -f E:/ws63-liteos-app_all.fwpkg

Example of Eraseing Commands：
-l                     loader file
BurnToolCmd.exe --erase -n ws63 -m serial COM10 --baudRate 115200  -l E:/ws63-liteos-app_all.fwpkg

Example of upload Commands
-l                     loader file
-f                     upload file
BurnToolCmd.exe --read -n ws63 -m serial COM10 -a 0x200000 -r 0x780  -l E:/ws63-liteos-app_all.fwpkg  -f upload.bin

FQA
Description of the error codes returned when the burning fails:
Error code  1: The command parameters are incorrect.
Error code  2: Failed to import the file. For example, the config file, XML file, or program file fails to be imported.
Error code  3: Failed to enable the serial port or I2C device.
Error code  4: Failed to burn the fastboot.
Error code  5: Failed to burn the partition file.
Error code  6: Erase failed
Error code  7: Failed to read data from board to file
Error code  8: Null pointer exception
Error code  9: New or malloc failed
Error code  10: Failed to send command


