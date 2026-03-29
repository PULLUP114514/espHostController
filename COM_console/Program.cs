using System;
using System.Diagnostics;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Text;
using static System.Runtime.InteropServices.JavaScript.JSType;
class Program
{
		static void Main()
		{

				// 捕获ctrlC
				Console.TreatControlCAsInput = true;

				SerialPort sp = new SerialPort(
						"COM6",
						115200
						);
				sp.Open();
				byte[] buffer = new byte[1024];
				while (true)
				{
						if (sp.BytesToRead > 0)
						{
								int n = sp.Read(buffer, 0, buffer.Length);		// 多字节支持
								string text = Encoding.UTF8.GetString(buffer, 0, n);
								Console.Write(text);
						}
						else if (Console.KeyAvailable)
						{
								var key = Console.ReadKey(true);
								byte[] data;

								switch (key.Key)
								{
										case ConsoleKey.UpArrow: data = new byte[] { 0x1B, 0x5B, 0x41 }; break;
										case ConsoleKey.DownArrow: data = new byte[] { 0x1B, 0x5B, 0x42 }; break;
										case ConsoleKey.RightArrow: data = new byte[] { 0x1B, 0x5B, 0x43 }; break;
										case ConsoleKey.LeftArrow: data = new byte[] { 0x1B, 0x5B, 0x44 }; break;
										default:
												if (key.KeyChar != 0)
												{
														data = System.Text.Encoding.UTF8.GetBytes(new char[] { key.KeyChar });
												}
												else
												{
														data = Array.Empty<byte>();
												}
												break;
								}

								if (data.Length > 0)
										sp.Write(data, 0, data.Length);
						}

				}
		}
}