#pragma once


typedef int BOOL;

struct TargetProcess
{
    int ProcessId;
};

struct TargetCallback
{
    int Index;
};


typedef struct _OBJ_CALLBACK_INFORMATION
{
    CHAR   ModuleName[256];
    ULONG64 Pointer;
    OB_OPERATION Operations;
    BOOL Enable;

} OBJ_CALLBACK_INFORMATION, * POBJ_CALLBACK_INFORMATION;






