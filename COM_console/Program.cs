using System;
using System.IO.Ports;
using System.Text;

class Program
{
    static void Main()
    {
        // 捕获ctrlC
        Console.TreatControlCAsInput = true;

        SerialPort sp = new SerialPort("/dev/ttyUSB0", 115200);
        sp.Open();

        byte[] buffer = new byte[1024];
        bool exitRequested = false;
        bool ctrlAPressed = false; // 标记是否按下了Ctrl+A

        while (!exitRequested)
        {
            // 读取串口数据
            if (sp.BytesToRead > 0)
            {
                int n = sp.Read(buffer, 0, buffer.Length);
                string text = Encoding.UTF8.GetString(buffer, 0, n);
                Console.Write(text);
            }
            // 读取键盘输入
            else if (Console.KeyAvailable)
            {
                var key = Console.ReadKey(true);
                byte[] data;

                // 判断 Ctrl+A
                if (key.Key == ConsoleKey.A && key.Modifiers.HasFlag(ConsoleModifiers.Control))
                {
                    	return;
                }

                // 如果上一个是 Ctrl+A 且按下 x，则退出
                if (ctrlAPressed && key.Key == ConsoleKey.X)
                {
			exitRequested = true;
			break;
                }

                ctrlAPressed = false; // 其他键重置标志

                // 普通按键转换成字节
                switch (key.Key)
                {
                    case ConsoleKey.UpArrow: data = new byte[] { 0x1B, 0x5B, 0x41 }; break;
                    case ConsoleKey.DownArrow: data = new byte[] { 0x1B, 0x5B, 0x42 }; break;
                    case ConsoleKey.RightArrow: data = new byte[] { 0x1B, 0x5B, 0x43 }; break;
                    case ConsoleKey.LeftArrow: data = new byte[] { 0x1B, 0x5B, 0x44 }; break;
                    default:
                        data = key.KeyChar != 0 ? Encoding.UTF8.GetBytes(new char[] { key.KeyChar }) : Array.Empty<byte>();
                        break;
                }

                if (data.Length > 0)
                    	sp.Write(data, 0, data.Length);
            }
        }

        sp.Close();
        Console.WriteLine("程序已退出");
    }
}