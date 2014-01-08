using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Net;
using System.Net.Sockets;
using System.IO;


namespace Spolks.NetServer
{
    public class Server
    {
        #region Globals
        //    TcpListener tcpListener;
        //    UdpClient udpClient;
        Thread mainTcpThread;
        Thread mainUdpThread;
        private class ConnectionInfo
        {
            public Socket Socket;
            public Thread Thread;
        }
        private List<ConnectionInfo> _connections = new List<ConnectionInfo>(); //список юзеров
        static AutoResetEvent locker;
        #endregion
        #region Создание сервера
        public Server(int port)
        {
            StartServer(GetMyIp(), port);
        } 
        #endregion
        #region Запуск сервера
        void StartServer(IPAddress ip, int port)
        {
            Console.WriteLine("Server started on: " + ip.ToString() + ":" + port);
            mainTcpThread = new Thread(() => MainTcpThread(ip, port));
            mainTcpThread.Start();
            mainUdpThread = new Thread(() => MainUdpThread(ip, port));
            mainUdpThread.Start();
        }
        #endregion
        #region Функция получения ip
        IPAddress GetMyIp()
        {
            string host = Dns.GetHostName();
            IPAddress ip = Dns.GetHostByName(host).AddressList[0];
            return ip;
        }
        #endregion
        #region Работа с TCP клиентом
        static void TCPClientWork(Socket _tcpClient)
        {
            Console.WriteLine("[" + _tcpClient.RemoteEndPoint.ToString() + "]: New TCP connection");
            #region Анализ файлов, инициализация передачи
            int socketResult;
            byte[] recvbuff = new byte [1024];
            int ByteReceived = 0;  
           // NetworkStream netStream = _tcpClient.GetStream();

            try {ByteReceived = _tcpClient.Receive(recvbuff);} //получение имени файла
            catch(Exception ex){}

            string filename = Encoding.Default.GetString(recvbuff);
            filename = filename.Substring(0, ByteReceived);
            string filepath = "C:\\" + filename;
            
           /* if(!File.Exists(filepath)) //если новый файл
            {   
                File.Create(filepath);
            }*/
            FileStream _fileStream = File.OpenWrite(filepath);
            FileInfo fileInfo = new FileInfo (filepath);
            int fileLength = Convert.ToInt32(fileInfo.Length);
            Console.WriteLine("Длина файла на сервере " + fileLength);
            _fileStream.Seek(fileLength, SeekOrigin.Current); //смещаем для докачки
            byte [] msg = Encoding.UTF8.GetBytes(fileLength.ToString());

            try {_tcpClient.Send(msg);} //отправка длины файла
            catch(Exception ex){} 

            recvbuff = new byte [1024];
            try { ByteReceived = _tcpClient.Receive(recvbuff); } //получение длины файла на клиенте
            catch (Exception ex) { }

            string tmp = Encoding.Default.GetString(recvbuff);
            int clientFileLength = Convert.ToInt32(tmp.Substring(0, ByteReceived));
            _tcpClient.ReceiveTimeout = 1;
            _tcpClient.SendTimeout = 1;
            #endregion
            #region Передача файла
            int x = 1;
            while (fileLength < clientFileLength) //крутим пока клиент не отключится 10000 !_tcpClient.Poll(0, SelectMode.SelectError)
            {
                
               byte[] fileBuf = new byte [1024];
               byte[] oobBuf = new byte[1];
               
               ArrayList lll = new ArrayList();
               lll.Add(_tcpClient);
               #region Срочные данные
               ByteReceived = 0;

               if (_tcpClient.Poll(1, SelectMode.SelectError))
               {
                   Console.WriteLine("OOB");
                   try { ByteReceived = _tcpClient.Receive(oobBuf, SocketFlags.OutOfBand); } //проверяем срочные
                   catch (Exception ex) { }

                   if (ByteReceived != 0) //если есть срочные данные то шлём обратно
                   {
                       _tcpClient.Send(oobBuf, SocketFlags.OutOfBand);
                   }
               }
               #endregion
               #region Файловые данные
               if (_tcpClient.Poll(1, SelectMode.SelectRead)) //если есть инфа )
               {
                   ByteReceived = 0;

                   try { ByteReceived = _tcpClient.Receive(fileBuf); } //порция файла
                   catch (Exception ex) { }

                   if (ByteReceived != 0) //если что-то пришло
                   {
                       _fileStream.Write(fileBuf, 0, ByteReceived);
                   }
                   string _tmp = Encoding.Default.GetString(fileBuf);
                   
               }
               fileLength += ByteReceived;
               //Console.WriteLine(x + ": " + fileLength + "rb " + ByteReceived); x++;
                #endregion
            }
            Console.WriteLine("[" + _tcpClient.RemoteEndPoint.ToString() + "]: Succesfully uploaded file " + filename);
            _fileStream.Close();
            _tcpClient.Close();
            return;
            #endregion
        }
        #endregion
        #region Работа с UDP клиентом
        static void UDPClientWork(object _obj) //UdpClient _client
        {
            UdpClient _client = (UdpClient)_obj;
            
        }
        #endregion
        #region Поток приёма TCP сокетов
        static void MainTcpThread(IPAddress ip, int port)
        {
            TcpListener tcpListener = new TcpListener(ip, port);
            tcpListener.Start();
            Console.WriteLine("TCP Thread - OK");
            while(true)
            {
                
               // TcpClient tcpClient = tcpListener.AcceptTcpClient();
                Socket tcpClient = tcpListener.AcceptSocket();
                Thread Thread = new Thread(()=> TCPClientWork(tcpClient));
                Thread.Start();
            }
        }
            #endregion
        #region Поток приёма UDP сокетов
        static void MainUdpThread(IPAddress ip, int port)
        {
            Socket mainUdpSocket;
            mainUdpSocket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            try
            {mainUdpSocket.Bind(new IPEndPoint (ip, port));}
            catch (Exception ex)
            {throw new Exception("Error occured while creating UDP server. Sys Info: ", ex);}
            ArrayList sockets = new ArrayList();
            sockets.Add(mainUdpSocket);
            ArrayList userList = new ArrayList(); //список активных коннектов
            List<ConnectionInfo> _connections = new List<ConnectionInfo>();
            Console.WriteLine("UDP Thread - OK");
            while(true)
            {
                if ((mainUdpSocket.Poll(0, SelectMode.SelectWrite)) && (mainUdpSocket.Connected)) //если кто-то подключился и с ним можно поговорить
                {
                    //в новый поток его и не паримся
                        ConnectionInfo connection = new ConnectionInfo();
                        connection.Socket = mainUdpSocket; //типа должно самообосраться через duplicateandclose
                        connection.Thread = new Thread(UDPClientWork);
                        connection.Thread.IsBackground = true;
                        connection.Thread.Start(connection);
                        lock (locker) _connections.Add(connection);
                    //делаем место для следующего подключения    
                       // ThreadPool.QueueUserWorkItem(new WaitCallback(UDPClientWork), newUdpClient);
                    }   

                }
            }
        }
        #endregion
        #region comments
    // _fileStream.Seek(0, SeekOrigin.End);
    //  int fileLength = Convert.ToInt32(_fileStream.Position); //длина файла на сервере


    //_tcpClient.Available - данные из сети


    //if(netStream.CanWrite){}
    /*
        Byte[] sendBytes = Encoding.UTF8.GetBytes("Connected");
       try{ netStream.Write(sendBytes, 0, sendBytes.Length);} //посылаем сигнал соединия
        catch(Exception ex) { _tcpClient.Close(); return;}
     */

    //приём смещения
        #endregion

    class Program
    {
        static void Main(string[] args)
        {
            #region StartInfo
            int maxThreadsCount = Environment.ProcessorCount * 10;
            ThreadPool.SetMaxThreads(maxThreadsCount, maxThreadsCount);
            ThreadPool.SetMinThreads(2, 2);
            #endregion
            Server _server = new Server(Convert.ToInt32(args[0]));
        }
    }
}
