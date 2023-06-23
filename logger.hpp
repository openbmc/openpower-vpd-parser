#pragma once

#include<iostream>
#include<fstream>
#include<string>
#include<filesystem>

class Logger
{
private:
    int msglevel;
    int inputlevel;
    std::fstream fin, fout, fwrite;
    enum Loglevel
    {
        ERROR,
        INFO,
        WARN,
        CRITICAL
    };

public:
    Logger()
    {
        if (std::filesystem::exists("/tmp/log_level.txt"))
        {
            fin.open("/tmp/log_level.txt", std::ios::in | std::ios::app | std::ios::out);
            fin >> inputlevel;
            if (inputlevel >= 4)
            {
                inputlevel = 0;
                fin.seekp(0, std::ios::beg);
                fin << 0;
                fout.open("/tmp/log_output.txt", std::ios::out | std::ios::app | std::ios::in);
                fout << "The input provided by you is out of range, so default level output is provided\n";
                fout.close();
            }
        }
        else
        {
            fin.open("/tmp/log_level.txt", std::ios::in | std::ios::app | std::ios::out);
            fin << "0\n\n\n/*choose your log level between 0-3\n0 indicates Error\n1 indicates Info\n2 indicates Warnings\n3 indicates critical messages*/";
            inputlevel = 0;
        }
        fin.close();
    }

    Logger& set_msglevel(int msg, std::string level, const size_t line, const std::string& file)
    {
        msglevel = msg;
        fwrite.open("/tmp/log_output.txt", std::ios::out | std::ios::app | std::ios::in);
        if (inputlevel == msglevel)
        {
            fwrite << "[" << level << "] " << __DATE__ << " " << __TIME__
                << " " << std::filesystem::path(file).filename() << ": " << line << " ";
        }
        fwrite.close();
        return *this;
    }

    template<class T>
    Logger& operator<<(const T& msg)
    {
        if (msglevel == inputlevel)
        {
            fwrite.open("/tmp/log_output.txt", std::ios::out | std::ios::app | std::ios::in);
            if (fwrite.is_open())
            {
                fwrite << msg;
                fwrite.close();
            }
        }
        return *this;
    }
    ~Logger() {}

};

#define VPD_ERROR Logger().set_msglevel(0,"ERROR",__LINE__,__FILE__)
#define VPD_INFO Logger().set_msglevel(1,"INFO",__LINE__,__FILE__)
#define VPD_WARN Logger().set_msglevel(2,"WARN",__LINE__,__FILE__)
#define VPD_CRITICAL Logger().set_msglevel(3,"CRITICAL",__LINE__,__FILE__)
