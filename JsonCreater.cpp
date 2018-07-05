// JsonCreater.cpp : 定义控制台应用程序的入口点。
//
#include "stdio.h"
#include <string>  
#include <iostream>  
#include <fstream> 
#include<algorithm> 
#include <sstream> 
#include "json/json.h"

using namespace std;
using std::cout;

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct StageInfo
{
    int nStart = -1;
    int nStop = -1;
}STAGEINFO;

typedef struct TeacherInfo
{
    std::string sTeacherId;
    long long nFirstFrameTime;
}TEACHERINFO;

typedef struct DoubleStageInfo
{
    int nStudentStart = -1;
    int nTeacherStart = -1;
    int nDuration = -1;
    std::string sTeacherId;
}DOUBLESTAGEINFO;

Json::Value eventJson;
Json::Value frameJson;
Json::Value resultJson;
Json::Value stateJson;

/*
#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif
#include<iostream>
 
std::string GetCurrentWorkingDir( void ) {
  char buff[FILENAME_MAX];
  GetCurrentDir( buff, FILENAME_MAX );
  std::string current_working_dir(buff);
  return current_working_dir + "/";
}

std::string getLocalDir()
{
    static std::string sLocalDir = "";
    if (sLocalDir.empty())
    {
#ifdef _WIN32
        char chpath[MAX_PATH];
        if (GetModuleFileNameA(NULL, (LPSTR)chpath, sizeof(chpath))) {
            chpath[strrchr(chpath, '\\') - chpath + 1] = 0;
        }
        sLocalDir = chpath;
#else
        //TODO:Linux下获取当前目录
#endif

    }
    return sLocalDir;
}
*/

std::map<std::string, long long> getFirstFrameTimeList()
{
    static std::map<std::string, long long> firstFrameMap;
    if (firstFrameMap.size() <= 0)
    {
        int nSize = frameJson.size();
        if (nSize > 0)
        {
            long long nFirstFrameTime = 0;
            for (auto frame : frameJson)
            {
                if (!frame.isMember("serverTs") || !frame.isMember("data") || !frame.isMember("type"))
                {
                    continue;
                }
                std::string userId = frame["data"].asString();
                if (frame["type"].asString() == "firstVideoFrame" && firstFrameMap.find(userId) == firstFrameMap.end())
                {
                    nFirstFrameTime = std::stoll(frame["serverTs"].asString());
                    firstFrameMap.insert(std::pair<std::string, long long>(userId,nFirstFrameTime));
                }
            }
        }
    }
    return firstFrameMap;
}

TEACHERINFO getCurrentTeacherFirstFrame(long long nServerTs)
{
    static std::map<long long, std::string> teacherFirstFrameMap;
    if (teacherFirstFrameMap.size() <= 0)
    {
        std::map<std::string, long long> firstFrameMap = getFirstFrameTimeList();
        Json::Value students = stateJson["result"]["students"];
        for (auto student : students)
        {
            if (student.isMember("studentId") && student["studentId"].isInt())
            {
                int nStudentId = student["studentId"].asInt();
                std::string studentStr = std::to_string(nStudentId);
                firstFrameMap.erase(studentStr);
            }
        }
        for (auto firstFrame : firstFrameMap)
        {
            teacherFirstFrameMap.insert(std::pair<long long, std::string>(firstFrame.second, firstFrame.first));
        }
    }
    long long nCurrentTeacherFirstFrame = -1;
    std::string sTeacherId = "";
    for (auto teacher : teacherFirstFrameMap)
    {
        if (nServerTs > teacher.first)
        {
            nCurrentTeacherFirstFrame = teacher.first;
            sTeacherId = teacher.second;
        }
        else
            break;
    }
    TeacherInfo info;
    info.nFirstFrameTime = nCurrentTeacherFirstFrame;
    info.sTeacherId = sTeacherId;
    return info;
}


void getStageResultTypeTwo()
{
    std::map<long long, Json::Value> stageMap;
    int nSize = eventJson.size();
    if (nSize > 0)
    {
        //使用 map 用serverTs进行排序
        for (int i = 0; i < nSize; ++i)
        {
            Json::Value event = eventJson[i];
            if (event.isMember("type") && event["type"].isString())
            {
                std::string type = event["type"].asString();// 
                if (type.find("stage") != std::string::npos
                    || type.find("upStage") != std::string::npos
                    || type.find("downStage") != std::string::npos)
                {
                    long long nServerTs = std::stoll(event["serverTs"].asString());
                    stageMap.insert(std::pair<long long, Json::Value>(nServerTs, event));
                }
            }
        }
        //stageInfoMap 存储单次上下台事件（学生ID为key，上下台事件时间组成value）
        std::map<int, DoubleStageInfo> stageInfoMap;
        std::map<std::string, long long> firstFrameMap = getFirstFrameTimeList();
        //Json::Value stageArray;
        for (auto it = stageMap.begin(); it != stageMap.end(); ++it)
        {
            Json::Value event = it->second;
            //stageArray.append(event);
            std::string sStage = event["type"].asString();
            std::string sData = event["data"].asString().c_str();
            long long nServerTs = std::stoll(event["serverTs"].asString());
            if (sStage == "stage") {
                if (sData == "false") {
                    for (auto it = stageInfoMap.begin(); it != stageInfoMap.end(); ++it)
                    {
                        int nData = it->first;
                        std::stringstream stream;
                        stream << it->first;
                        std::string sData = stream.str();
                        std::map<std::string, long long>::iterator firstFrame = firstFrameMap.find(sData);
                        //存储上下台事件时间到JSON
                        int size = resultJson.size();
                        stageInfoMap[nData].nDuration = nServerTs - firstFrame->second - stageInfoMap[nData].nStudentStart;
                        Json::Value stage;
                        stage["studentstart"] = stageInfoMap[nData].nStudentStart;
                        stage["teacherstart"] = stageInfoMap[nData].nTeacherStart;
                        stage["duration"] = stageInfoMap[nData].nDuration;
                        stage["teacherid"] = stageInfoMap[nData].sTeacherId;

                        int nSize = resultJson[sData].size();
                        resultJson[sData][nSize] = stage;
                    }
                }
            }
            else {
                std::map<std::string, long long>::iterator firstFrame = firstFrameMap.find(sData);
                TeacherInfo info = getCurrentTeacherFirstFrame(nServerTs);
                long long nCurrentTeacherFirstFrame = info.nFirstFrameTime;
                if (firstFrame != firstFrameMap.end())
                {
                    int nFrom = atoi(event["from"].asString().c_str());
                    int nData = atoi(sData.c_str());
                    if (nFrom == nData)
                    {
                        continue;
                    }
                    std::map<int, DoubleStageInfo>::iterator stageInfo = stageInfoMap.find(nData);
                    if (sStage == "upStage")
                    {
                        if (stageInfo == stageInfoMap.end())
                        {
                            //增加一个新的上台事件时间
                            DoubleStageInfo info;
                            stageInfoMap.insert(std::pair<int, DoubleStageInfo>(nData, info));
                        }
                        stageInfoMap[nData].nStudentStart = nServerTs - firstFrame->second;
                        stageInfoMap[nData].nTeacherStart = nServerTs - nCurrentTeacherFirstFrame;
                        stageInfoMap[nData].sTeacherId = info.sTeacherId;
                    }
                    else
                    {
                        if (stageInfo == stageInfoMap.end())
                        {
                            //如果未存储该上台事件的上台事件，忽略下台事件
                            continue;
                        }
                        else
                        {
                            //存储上下台事件时间到JSON
                            int size = resultJson.size();
                            stageInfoMap[nData].nDuration = nServerTs - firstFrame->second - stageInfoMap[nData].nStudentStart;
                            Json::Value stage;
                            stage["studentstart"] = stageInfoMap[nData].nStudentStart;
                            stage["teacherstart"] = stageInfoMap[nData].nTeacherStart;
                            stage["duration"] = stageInfoMap[nData].nDuration;
                            stage["teacherid"] = stageInfoMap[nData].sTeacherId;
                            stageInfoMap.erase(stageInfo);

                            std::stringstream stream;
                            stream << nData;
                            std::string sData = stream.str();
                            int nSize = resultJson[sData].size();
                            resultJson[sData][nSize] = stage;
                        }
                    }
                }
            }
        }
        //std::string sArray = stageArray.toStyledString();
    }

}

bool parseJsonFileTypeTwo(const std::string &roomdir)
{
    bool bParseSuccess = false;
    std::string sLocalDir = roomdir;
    std::string sClassEvent = sLocalDir + "classevent.json";
    std::string sClassState = sLocalDir + "classstate.json";
    std::string sClassFrame = sLocalDir + "firstframe.json";

    std::string sStage = "stage.json";

    std::ifstream fileEvent, fileFrame,fileState;
    fileEvent.open(sClassEvent.c_str(), std::ios::in);
    fileState.open(sClassState.c_str(), std::ios::in);
    fileFrame.open(sClassFrame.c_str(), std::ios::in);
    if (!fileEvent.is_open() || !fileState.is_open() || !fileFrame.is_open())
    {
        return bParseSuccess;
    }
    Json::Reader reader;
    if (reader.parse(fileEvent, eventJson)
        && reader.parse(fileState, stateJson)
        && reader.parse(fileFrame, frameJson))
    {
        bParseSuccess = true;
    }
    fileEvent.close();
    fileState.close();
    fileFrame.close();

    return bParseSuccess;

}

//////////////////////////////////////////////////////////////////////////////////////////////////
bool parseJsonFileTypeOne(const std::string &roomdir)
{
    bool bParseSuccess = false;
    //cout <<"parseJsonFile"<< roomdir <<endl;
    std::string sLocalDir = roomdir;
    cout <<"parseJsonFile"<< sLocalDir <<endl;
    std::string sClassEvent = sLocalDir + "classevent.json";
    std::string sClassFrame = sLocalDir + "firstframe.json";

    std::string sStage = "stage.json";

    std::ifstream fileEvent, fileFrame;
    fileEvent.open(sClassEvent.c_str(), std::ios::in);
    fileFrame.open(sClassFrame.c_str(), std::ios::in);
    if (!fileEvent.is_open() || !fileFrame.is_open())
    {
        return bParseSuccess;
    }
    Json::Reader reader;
    if (reader.parse(fileEvent, eventJson)
        && reader.parse(fileFrame, frameJson))
    {
        bParseSuccess = true;
    }
    fileEvent.close();
    fileFrame.close();

    return bParseSuccess;

}

long long getFirstFrameTimeTypeOne()
{
    static long long nFristFrameTime = 0;
    if (nFristFrameTime == 0)
    {
        int nSize = frameJson.size();
        if (nSize > 0)
        {
            if (frameJson[0].isMember("serverTs"))
            {
                if (frameJson[0]["serverTs"].isString())
                {
                    nFristFrameTime = std::strtol(frameJson[0]["serverTs"].asString().c_str(),NULL,10);
                }
            }
        }
    }
    return nFristFrameTime;
}

void getStageResultTypeOne()
{
    std::map<long long, Json::Value> stageMap;
    int nSize = eventJson.size();
    if (nSize > 0)
    {
        //使用 map 用serverTs进行排序
        for (int i = 0;i < nSize;++i)
        {
            Json::Value event = eventJson[i];
            if (event.isMember("type") && event["type"].isString())
            {
                std::string type = event["type"].asString();// 
                if (type.find("stage") != std::string::npos 
					|| type.find("upStage") != std::string::npos 
					|| type.find("downStage") != std::string::npos)
                {
                    long long nServerTs = std::strtol(event["serverTs"].asString().c_str(),NULL,10);//_atoi64(event["serverTs"].asString().c_str());
                    stageMap.insert(std::pair<long long, Json::Value>(nServerTs,event));
                }
            }
        }
        //stageInfoMap 存储单次上下台事件（学生ID为key，上下台事件时间组成value）
        std::map<int, STAGEINFO> stageInfoMap;  
		//Json::Value stageArray;
        for (auto it = stageMap.begin(); it != stageMap.end(); ++it)
        {
            Json::Value event = it->second;
			//stageArray.append(event);
            std::string sStage = event["type"].asString();
			std::string sData = event["data"].asString().c_str();
			long long nServerTs = std::strtol(event["serverTs"].asString().c_str(),NULL,10);//_atoi64(event["serverTs"].asString().c_str());
			int nTime = nServerTs - getFirstFrameTimeTypeOne();
			if (sStage == "stage"){
				if (sData == "false"){
					for (auto it = stageInfoMap.begin();it != stageInfoMap.end(); ++it)
					{
						//存储上下台事件时间到JSON
						int size = resultJson.size();
						it->second.nStop = nTime;
						Json::Value stage;
						stage["starttime"] = it->second.nStart;
						stage["duration"] = it->second.nStop - it->second.nStart;

						std::stringstream stream;
						stream << it->first;
						std::string sData = stream.str();
						int nSize = resultJson[sData].size();
						resultJson[sData][nSize] = stage;
					}
				}
			}
			else {
				int nFrom = atoi(event["from"].asString().c_str());
				int nData = atoi(sData.c_str());
				if (nFrom == nData)
				{
					continue;
				}
				std::map<int, STAGEINFO>::iterator stageInfo = stageInfoMap.find(nData);
				if (sStage == "upStage")
				{
					if (stageInfo == stageInfoMap.end())
					{
						//增加一个新的上台事件时间
						STAGEINFO info;
						stageInfoMap.insert(std::pair<int, STAGEINFO>(nData, info));
					}
					stageInfoMap[nData].nStart = nTime;
				}
				else
				{
					if (stageInfo == stageInfoMap.end())
					{
						//如果未存储该上台事件的上台事件，忽略下台事件
						continue;
					}
					else
					{
						//存储上下台事件时间到JSON
						int size = resultJson.size();
						stageInfoMap[nData].nStop = nTime;
						Json::Value stage;
						stage["starttime"] = stageInfoMap[nData].nStart;
						stage["duration"] = nTime - stageInfoMap[nData].nStart;
						stageInfoMap.erase(stageInfo);

						std::stringstream stream;
						stream << nData;
						std::string sData = stream.str();
						int nSize = resultJson[sData].size();
						resultJson[sData][nSize] = stage;
					}
				}
			}
        }
		//std::string sArray = stageArray.toStyledString();
    }

}

int main(int argc, char *argv[])
{
    std::string strworkdir;
    int parsejsontype = 0;

    for(int i=0; i<argc; i++)
    {
        cout << i << argv[i] << endl;
    }
    if(argc ==3 )
    {
        strworkdir = argv[1];
        cout<< strworkdir <<endl;

        parsejsontype = std::stoi(argv[2]);
        cout << "type:" << parsejsontype <<endl;
    }
    else{
        cout<< " param is not correct!" <<endl;
        return -1;
    }

    //
    if(parsejsontype == 1)
    {
        resultJson.clear();
        cout <<"type:" << parsejsontype <<"dir:"<<strworkdir << endl;
        parseJsonFileTypeOne(strworkdir);
        getStageResultTypeOne();

        int nSize = resultJson.size();
        std::string sResult = resultJson.toStyledString();
        std::string sStagefilepath =  strworkdir + "stage.json";
        std::ofstream out(sStagefilepath.c_str());
        out << sResult.c_str();
        out.close();
    }
    else if(parsejsontype == 2)
    {
        resultJson.clear();
        cout <<"type:" << parsejsontype <<"dir:"<<strworkdir << endl;
        parseJsonFileTypeTwo(strworkdir);

        getStageResultTypeTwo();

        int nSize = resultJson.size();
        std::string sResult = resultJson.toStyledString();
        //std::string sLocalDir = getLocalDir();
        std::string sStage = strworkdir + "stagenew.json";
        std::ofstream out(sStage.c_str());
        out << sResult.c_str();
        out.close();
        //return 0;
    }

    return 0;
}

