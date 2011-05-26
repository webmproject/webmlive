#include <windows.h>
#include <iostream>
#include <cassert>
#include <sstream>
#include <numeric>

#include "livehttpposter.hpp"

using namespace std;

extern HANDLE g_handle_quit;

namespace live_webm_streamer {

long LivePoster::size_file_ = 0;
bool LivePoster::init_ = false;
bool LivePoster::codec_private_checked_ = false;
double LivePoster::max_bps_ = 0.0;
double LivePoster::min_bps_ = 0.0;
double LivePoster::cur_bps_ = 0.0;

wchar_t* LivePoster::ip_address_= NULL;
wchar_t* LivePoster::port_ = NULL;
wchar_t* LivePoster::webm_file_ = NULL;

deque<double> LivePoster::dq_;

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
  struct curl_slist* headerlist = NULL;
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
        ofstream ofs_multipart;

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

        ofs_multipart.open("multipart.webm", ios_base::binary | ios_base::out);

        char* const buffer = new char[size_diff];
        ifs.read(buffer, size_diff);
        ifs.close();

        if (!codec_private_checked_)
        {
          //TODO(hwasoo): consider A_VORBIS is the last part of the buffer.
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

              if ((*(buffer + i + 8) == 99)     // 0x63
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
          ofs_multipart.write(buffer, size_diff);
          ofs_multipart.close();

          if (curl_)
          {
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

            cout << "  multipart size(uploading): " << size_diff << " Byte(s)" << endl;
            res_ = curl_easy_setopt(curl_, CURLOPT_HTTPPOST, formpost);

            cout.width(10);
            cout.precision(4);

            if (cur_bps_ != 0.0)
              cout << "  estimated upload time in second : " << (size_diff / 1000) / cur_bps_ << endl;

            __int64 ctr1 = 0, ctr2 = 0, freq = 0;
            QueryPerformanceCounter((LARGE_INTEGER*)&ctr1);

            res_ = curl_easy_perform(curl_);

            if (res_ == CURLE_OK)
            {
              QueryPerformanceCounter((LARGE_INTEGER*)&ctr2);
              QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
              const double span = (ctr2 - ctr1)* 1.0 /freq;
              cout << "  real upload time in second : " << span << endl;

              cur_bps_ = (double)(((double)size_diff / span) / 1000.0);

              if (!init_)
              {
                init_ = true;
                min_bps_ = cur_bps_;
              }

              if (cur_bps_ > max_bps_)
                max_bps_ = cur_bps_;

              if (cur_bps_ < min_bps_)
                min_bps_ = cur_bps_;

              cout << "  " << cur_bps_ << " KBps  <--- current speed" << endl;
              dq_.push_back(cur_bps_);

              if (dq_.size() > 100)
                dq_.pop_front();

              const double avg_bps = (accumulate(dq_.begin(), dq_.end(), 0.0)) / dq_.size();

              cout << "  Max(KBps) : " << max_bps_ << "   Avg(KBps) : " << avg_bps << "   Min(KBps) : " << min_bps_ << endl << endl;

              if (res_ == CURLE_COULDNT_CONNECT)
                cout << " Failed to connect() to host or proxy " << endl;
            }
            else
            {
              //TODO(hwasoo): need to exit out this thread
              ;
            }
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