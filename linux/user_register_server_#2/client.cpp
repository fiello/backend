
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>

using namespace std;

void usage(const char *filename)
{
   cerr << endl;
   cerr << "Wrong input arguments." << endl;
   cerr << "Usage: " << filename << " [-u] [-f] <server_ip> <server_port> <requests_file>\n";
   cerr << "  -u              : send to UDP connection instead of TCP" << endl;
   cerr << "  -f              : don't wait for response. For TCP - close connection after each send." << endl;
   cerr << "  <requests_file> : file with requests. one line per request" << endl;
   cerr << endl;
}


int main(int argc, char *argv[])
{
   bool tcp = true, force=false;
   char opt;
   while ((opt = getopt(argc, argv, "uf")) != -1) {
      switch (opt) {
         case 'u':
            tcp = false;
            break;
         case 'f':
            force = true;
            break;
         default: /* '?' */
            usage(argv[0]);
            exit(EXIT_FAILURE);
      }
   }

   if (optind + 3 > argc) {
      usage(argv[0]);
      exit(EXIT_FAILURE);
   }

   const char *ip = argv[optind];
   int port = atoi(argv[optind+1]);
   const char *requests_file = argv[optind + 2];
   
   cout << "Creating " << (const char *)(tcp ? "TCP" : "UDP") << " client to " << ip << ":" << port << endl;
   ifstream reqStream(requests_file);
   int sock = -1;
   while(reqStream)
   {
      std::string line;
      getline(reqStream, line);
      int size = line.size();
      if(line[size - 1] == '\n')
         size--;
      if(line[size - 1] == '\r')
         size--;
      
      line.resize(size);
      
      cout << "Sending line: " << line << endl;

      line += "\r\n";

      if(sock == -1)
      {
         sock = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
         struct sockaddr_in addr;
         addr.sin_family = AF_INET;
         addr.sin_port = htons(port);
         inet_aton(ip, &addr.sin_addr);
         if(-1 == connect(sock, (const sockaddr*)&addr, sizeof(struct sockaddr_in)))
         {
            cerr << "Failed to connect to server" << endl;
            exit(EXIT_FAILURE);
         }
      }

      send(sock, line.c_str(), line.size(), 0);

      if(!force)
      {
         char buf[1024];
         int readBytes = recv(sock, buf, sizeof(buf), 0);
         if(readBytes < 0 || readBytes >= sizeof(buf))
         {
            cout << "Failed to read data (connection closed?)" << endl;
            break;
         }
         buf[readBytes] = '\0';
         cout << buf;
      }
      else
      {
         if(tcp)
         {
            close(sock);
            sock = -1;
         }
      }
   }
   exit(EXIT_SUCCESS);
}
