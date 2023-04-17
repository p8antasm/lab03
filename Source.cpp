#include "windows.h"
#include <string>
#include <iostream>
#include <iomanip>

using namespace std;

#pragma pack(push, 1)

typedef struct
{
    BYTE jump[3];
    BYTE name[8];
    INT16 bytesPerSector;
    BYTE sectorPerCluster;
    BYTE reserved1[7];
    BYTE mediaDesc;
    INT16 reserved2;
    INT16 sectorPerTrack;
    INT16 numHeads;
    BYTE reserved3[12];
    INT64 numSectors;
    INT64 mftCluster;     // первый кластер MFT
    INT64 mftMirrCluster; // первый кластер MFTMirr
    INT32 mftSize;        // количество кластеров
    BYTE clustersPerIndex;
    BYTE reserved4[3];
    INT64 volume_id;
    INT32 checksum;
    BYTE buf[428];
} NtfsBootSector;

#pragma pack(pop)

INT64 SetFileSeek(HANDLE h, INT64 distance, INT16 seekMethod);
VOID PrintHex(CONST BYTE *data, INT size);
INT ReadCluster(HANDLE hDisk, BYTE *destination, UINT64 clusterNumber, UINT16 clusterSize);

INT main()
{
    for (WCHAR driveLetter = L'A'; driveLetter <= L'Z'; ++driveLetter)
    {
        WCHAR partitionPath[8];
        swprintf_s(partitionPath, L"\\\\.\\%c:", driveLetter);

        wstring path(partitionPath);

        HANDLE hPartition = CreateFileW(            // Имя файла (WCHAR*)
            path.c_str(),                           // Режим доступа
            GENERIC_READ,                           // Режим совместной работы
            FILE_SHARE_READ | FILE_SHARE_WRITE,     // Атрибуты безопасности
            NULL,                                   // Способ открытия
            OPEN_EXISTING,                          // Флаги и атрибуты
            FILE_ATTRIBUTE_NORMAL,                  // Описатель (идентификатор) файла шаблона с правами доступа GENERIC_READ.
            NULL
        );

        if (hPartition == INVALID_HANDLE_VALUE)
        {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_FILE_NOT_FOUND)
            {
               ;
            }
            else
            {
                wcerr << L"Failed to open drive " << driveLetter << ": " << lastError << endl;
            }
        }
        else
        {
            cout << "Drive " << (char)driveLetter << " found" << endl;
            CloseHandle(hPartition);
        }
    }

    INT result;
    wstring volumeName;

    cout << "Enter volume name: ";
    wcin >> volumeName;
    wstring filename = L"\\\\.\\" + volumeName + L":";
    HANDLE hDisk = CreateFileW(
        filename.c_str(),                           // Имя файла (WCHAR*)
        GENERIC_READ,                               // Режим доступа
        FILE_SHARE_READ | FILE_SHARE_WRITE,         // Режим совместной работы
        NULL,                                       // Атрибуты безопасности
        OPEN_EXISTING,                              // Способ открытия
        FILE_ATTRIBUTE_NORMAL,                      // Флаги и атрибуты
        NULL                                       // Описатель (идентификатор) файла шаблона с правами доступа GENERIC_READ.
    );

    if (hDisk == INVALID_HANDLE_VALUE)
    {
        wcerr << L"Error opening file " << filename.c_str();
        exit(1);
    }
    WCHAR fstype[MAX_PATH + 1];
    result = GetVolumeInformationByHandleW(hDisk, NULL, 0, 0, 0, 0, fstype,
                                           MAX_PATH + 1);
    if (result == 0)
    {
        cerr << "Error in GetVolumeInformationByHandleW\n";
        CloseHandle(hDisk);
        exit(EXIT_FAILURE);
    }
    if (wmemcmp(fstype, L"NTFS", 4) != 0)
    {
        cerr << "NTFS not found\n";
        CloseHandle(hDisk);
        exit(EXIT_FAILURE);
    }
    NtfsBootSector ntfsBoot;
    DWORD bytesToRead = 512;
    DWORD bytesRead;
    INT64 position = SetFileSeek(hDisk, 0, FILE_BEGIN);

    if (position == -1)
    {
        cerr << "Error SetFileSeek\n";
        CloseHandle(hDisk);
        exit(EXIT_FAILURE);
    }

    BOOL readResult = ReadFile(hDisk, &ntfsBoot, bytesToRead, &bytesRead, NULL);
    if (!readResult || bytesRead != bytesToRead)
    {
        cerr << "Error read file";
        CloseHandle(hDisk);
        exit(EXIT_FAILURE);
    }

    UINT16 clusterSize = ntfsBoot.bytesPerSector * ntfsBoot.sectorPerCluster;
    UINT64 numClusters = ntfsBoot.numSectors / ntfsBoot.sectorPerCluster;
    cout << "NTFS Boot Sector in hex:\n\t____________________________________\n";
    PrintHex((CONST BYTE *)&ntfsBoot, 512);
    cout << "\t------------------------------------\n";
    cout << "NTFS Info:" << endl;
    cout << "Sector size: " << ntfsBoot.bytesPerSector << " bytes" << endl;
    cout << "Total Sectors count: " << ntfsBoot.numSectors << endl;
    cout << "Cluster size: " << clusterSize << " bytes" << endl;
    cout << "Total Clusters count: " << numClusters << endl;
    cout << "Logical Cluster Number for the file $MFT: " << ntfsBoot.mftCluster << endl;
    cout << "Logical Cluster Number for the file $MFTMirr: " << ntfsBoot.bytesPerSector * ntfsBoot.mftMirrCluster << endl;
    cout << "MFT size: " << ntfsBoot.mftSize << " clusters" << endl;
    cout << "Volume id: " << ntfsBoot.volume_id << endl;
    
    UINT64 lcn; // Logical Cluster Number
    cout << "Enter Logical Cluster Number: ";
    cin >> lcn;
    if (cin.fail())
    {
        cerr << "INT is needed. Exiting." << endl;
        exit(EXIT_FAILURE);
    }
    if (lcn >= numClusters)
    {
        cerr << "No such LCN";
        CloseHandle(hDisk);
        exit(1);
    }
    BOOL needPrintCluster = FALSE;
    INT temp;
    cout << "Print cluster[1 - yes, 0 - no]: ";
    cin >> temp;
    needPrintCluster = (temp == 0) ? FALSE : TRUE;
    BYTE *buffer = new BYTE[clusterSize];
    result = ReadCluster(hDisk, buffer, lcn, clusterSize);
    if (result != 0)
    {
        cerr << "Error reading cluster";
        CloseHandle(hDisk);
        delete[] buffer;
        exit(1);
    }
    if (needPrintCluster)
    {
        PrintHex(buffer, clusterSize);
    }
    delete[] buffer;
    CloseHandle(hDisk);
    return 0;
}
INT64 SetFileSeek(HANDLE h, INT64 distance, INT16 seekMethod)
{
    LARGE_INTEGER li;

    li.QuadPart = distance;

    li.LowPart = SetFilePointer(h, li.LowPart, &li.HighPart,
                                seekMethod);
    INT32 error = GetLastError();
    if (li.LowPart == INVALID_SET_FILE_POINTER && error != NO_ERROR)
    {
        li.QuadPart = -1;
    }

    return li.QuadPart;
}
VOID PrintHex(CONST BYTE *data, INT size)
{
    ios init(NULL);
    init.copyfmt(cout);
    INT pos = 0;
    INT offset = 0;
    while (pos < size)
    {
        cout << std::dec << setw(8) << offset << "|";
        for (INT j = 0; j < 16; ++j)
        {
            cout << std::hex << setw(2) << setfill('0') << uppercase << INT(data[pos]) << ' ';
            ++pos;
        }
        cout << endl;
        offset += 16;
    }
    cout.copyfmt(init);
}
INT ReadCluster(HANDLE h, BYTE *destination, UINT64 clusterNumber, UINT16 clusterSize)
{
    INT64 position = SetFileSeek(h, clusterNumber * clusterSize, FILE_BEGIN);
    if (position == -1)
    {
        cerr << "[ReadCluster] error SetFileSeek" << endl;
        return -1;
    }
    DWORD bytesToRead = 512;
    DWORD bytesRead;
    BOOL readResult = ReadFile(
        h,
        destination,
        bytesToRead,
        &bytesRead,
        NULL);
    if (!readResult || bytesRead != bytesToRead)
    {
        cerr << "[ReadCluster] error read file" << endl;
        return -1;
    }
    return 0;
}