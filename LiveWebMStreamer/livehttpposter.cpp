#include <windows.h>
#include <iostream>
#include <cassert>
#include <sstream>

#include "livehttpposter.hpp"

using namespace std;

extern HANDLE g_handle_quit;

namespace live_webm_streamer {

long LivePoster::size_file_ = 0;
bool LivePoster::init_ = false;
bool LivePoster::codec_private_checked_ = false;
wchar_t* LivePoster::ip_address_= NULL;
wchar_t* LivePoster::port_ = NULL;
wchar_t* LivePoster::webm_file_ = NULL;

LivePoster::LivePoster(void) 
{
}

LivePoster::~LivePoster(void)
{
}

unsigned LivePoster::ThreadProc(void* pv)
{
  const HANDLE* ha = reinterpret_cast<const HANDLE*>(pv);
  
  CURL* curl_;
  CURLcode res_;
  
  const char* const clone_file = "test_clone.webm";
  
  struct curl_httppost* formpost = NULL;
  struct curl_httppost* lastptr = NULL;
  struct curl_slist* headerlist=NULL;
  static const char buf[] = "Expect:";
  
  curl_ = curl_easy_init();
  
  bool post_ready = false;
    
  for (;;)
  {
    const DWORD dw = WaitForMultipleObjects(2, ha, FALSE, INFINITE);

    if (dw == (WAIT_OBJECT_0 + 0))  // Ctrl+C
    {
      ifstream ifs(webm_file_, ios_base::binary | ios_base::in);
      
      if (ifs.is_open()) 
      {
        const bool b = ifs.good();
        assert(b);
        
        ifs.seekg(0, ios::end);
        const long size_temp = ifs.tellg();
        const long size_diff = size_temp - size_file_;
        
        ifs.seekg(size_file_, ios::beg);
        
        char* const buffer = new char[size_diff];
        ifs.read(buffer, size_diff);
        size_file_ = size_temp;
        ifs.close();
        
#ifdef _DEBUG        
        ofstream ofs(clone_file, ios_base::binary | ios_base::out | ios::app);
        ofs.write(buffer, size_diff);
        ofs.close();
#endif        
        delete [] buffer;
        
        curl_easy_cleanup(curl_);
        curl_formfree(formpost);
      }
      return 0;        
    }
    
    if (dw == (WAIT_OBJECT_0 + 1)) // POST event
    {
      ifstream ifs(webm_file_, ios_base::binary | ios_base::in);
      
      if (ifs.is_open()) 
      {
        const bool b = ifs.good();
        assert(b);
        
        ifs.seekg(0, ios::end);
        const long size_temp = ifs.tellg();
        const long size_diff = size_temp - size_file_;
        
        if (size_diff == 0)
        {
          ifs.close();
          continue;
        }

#ifdef _DEBUG        
        ofstream ofs;
#endif         
        ofstream ofs_1;

        if (size_file_ == 0) 
        {
          ifs.seekg(0, ios::beg);
#ifdef _DEBUG                  
          ofs.open(clone_file, ios_base::binary | ios_base::out);
#endif                   
        }
        else
        {
          ifs.seekg(size_file_, ios::beg);
#ifdef _DEBUG                  
          ofs.open(clone_file, ios_base::binary | ios_base::out | ios::app);
#endif                   
        }
        
        ofs_1.open("multipart.webm", ios_base::binary | ios_base::out);

        char* const buffer = new char[size_diff];
        ifs.read(buffer, size_diff);
        ifs.close();
        

//          for (int i = 0; i < size_diff - 3; ++i)
//          {
//            if ((*(buffer + i ) == -20)     // 0xEC - Void Element
//             && (*(buffer + i + 1) == 95)   // 0x5F
//             && (*(buffer + i + 2) == 61))  // 0x3D
//            {
//#ifdef _DEBUG
//              cout << endl << " Found Void element in Codec Private." << endl;
//#endif           
//              post_ready = true;
//              codec_private_checked_ = false;
//              break;
//            }
//          }
       
        if (!codec_private_checked_) 
        {
          for (int i = 0; i < size_diff - 10; ++i)
          {
            if ((*(buffer + i ) == 65)      // A
             && (*(buffer + i + 1) == 95)   // _
             && (*(buffer + i + 2) == 86)   // V
             && (*(buffer + i + 3) == 79)   // O
             && (*(buffer + i + 4) == 82)   // R
             && (*(buffer + i + 5) == 66)   // B
             && (*(buffer + i + 6) == 73)   // I
             && (*(buffer + i + 7) == 83))  // S
            {
              if ((*(buffer + i + 8) == -20)  // 0xEC - Void Element
               && (*(buffer + i + 9) == 95)   // 0x5F
               && (*(buffer + i + 10) == 61)) // 0x3D
              {
#ifdef _DEBUG
                cout << endl << " A_VORBIS and Void element found" << endl;
#endif           
                post_ready = true;
                break;
              }
              
              if ((*(buffer + i + 8) == 99)    // 0x63
                && (*(buffer + i + 9) == -94))  // 0xA2
              {
#ifdef _DEBUG
                cout << endl << " A_VORBIS and Codec Private found" << endl;
#endif           
                codec_private_checked_ = true;
                break;
              }
            }  
          } // for loop
        } // if codec_private_checked_

        // Check live cluster size
        for (int i = 0; i < size_diff - 7; ++i)
        //for (int i = 0; i < size_diff - 8; ++i)
        {
          if ((*(buffer + i) == 31)        //0x1F - Cluster 
           && (*(buffer + i + 1) == 67)    //0x43 - Cluster
           && (*(buffer + i + 2) == -74)   //0xB6 - Cluster
           && (*(buffer + i + 3) == 117)   //0x75 - Cluster
           && (*(buffer + i + 4) == 31)    //0x1F
           && (*(buffer + i + 5) == -1)    //0xFF 
           && (*(buffer + i + 6) == -1)    //0xFF  
           && (*(buffer + i + 7) == -1))   //0xFF  
          {
#ifdef _DEBUG
            cout << endl << " Cluster size is undefined." << endl;
#endif           
            post_ready = true;
            break;
          }
        }
      
        if (!post_ready)
        {
          size_file_ = size_temp;  
#ifdef _DEBUG          
          ofs.write(buffer, size_diff);
#endif                   
          ofs_1.write(buffer, size_diff);
          ofs_1.close();
        
          if (curl_)
          {
            //if (init_ == false) 
            //{
            //  curl_formadd(&formpost,
            //   &lastptr,
            //   CURLFORM_COPYNAME, "first",
            //   CURLFORM_COPYCONTENTS, "yes",
            //   CURLFORM_END); 

            //  curl_formadd(&formpost,
            //   &lastptr,
            //   CURLFORM_COPYNAME, "send_webm_stream",
            //   CURLFORM_FILE, "multipart.webm",
            //   CURLFORM_END);
            //  
            //  curl_easy_setopt(curl_, CURLOPT_URL, "http://127.0.0.1:8080");
            //  //curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, "first=yes&project=curl");
            //  res_ = curl_easy_setopt(curl_, CURLOPT_HTTPPOST, formpost); 
            //  res_ = curl_easy_perform(curl_);
            //  curl_formfree(formpost);
            //  //curl_formfree(lastptr);
            //  init_ = true;
            //}
            //else 
            //{
              //curl_formfree(formpost);
              //curl_formadd(&formpost,
              // &lastptr,
              // CURLFORM_COPYNAME, "first",
              // CURLFORM_COPYCONTENTS, "no",
              // CURLFORM_END); 

            curl_formadd(&formpost,
             &lastptr,
             CURLFORM_COPYNAME, "send_webm_stream",
             CURLFORM_FILE, "multipart.webm",
             CURLFORM_END);
             
            ostringstream os;
            os << "http://";
            
            const wchar_t* p = ip_address_;
            const wchar_t* q = p + wcslen(p);
            
            while (p != q)
              os << static_cast<char>(*p++);
              
            os << ':';
            
            p = port_;
            q = p + wcslen(p);
            
            while (p != q)
              os << static_cast<char>(*p++);
              
            const string str_ = os.str();
            const char* const str = str_.c_str();
            
            res_ = curl_easy_setopt(curl_, CURLOPT_URL, str);
            
            cout << "file size " << size_diff << endl;
            res_ = curl_easy_setopt(curl_, CURLOPT_HTTPPOST, formpost); 
           
            __int64 ctr1 = 0, ctr2 = 0, freq = 0;
            QueryPerformanceCounter((LARGE_INTEGER*)&ctr1);
           
            res_ = curl_easy_perform(curl_);
            QueryPerformanceCounter((LARGE_INTEGER*)&ctr2);    
	          QueryPerformanceFrequency((LARGE_INTEGER*)&freq);            
	          double span = (ctr2 - ctr1)* 1.0 /freq;
  	        
	          cout << "  " << size_diff / span << " BPS" << endl;
  	        
            if (res_ == CURLE_COULDNT_CONNECT)
              cout << " Failed to connect() to host or proxy " << endl;
          }
        }
        
        post_ready = false;
#ifdef _DEBUG          
        ofs.close();
#endif                   
        delete [] buffer;
      }
    } // POST event
  } // for(;;)
} // ThreadProc

}