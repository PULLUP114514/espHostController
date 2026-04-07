cd ~/home/bsodwtpc/myFiles/Proj/ePass/App/UartIpcController/
make
cp ./src/appconfig.json ./dist/UartController/
cp ./UartController ./dist/UartController/
scp -O -r ./dist/UartController/ root@192.168.137.2:/app/
