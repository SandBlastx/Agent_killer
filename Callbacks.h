#pragma once


using OB_CALLBACK_ENTRY = struct _OB_CALLBACK_ENTRY;
using OB_CALLBACK = struct _OB_CALLBACK;


typedef struct _OB_CALLBACK_ENTRY {
    LIST_ENTRY CallbackList; // linked element tied to _OBJECT_TYPE.CallbackList
    OB_OPERATION Operations; // bitfield : 1 for Creations, 2 for Duplications
    BOOL Enabled;            // self-explanatory
    OB_CALLBACK* Entry;      // points to the structure in which it is included
    POBJECT_TYPE ObjectType; // points to the object type affected by the callback
    POB_PRE_OPERATION_CALLBACK PreOperation;      // callback function called before each handle operation
    POB_POST_OPERATION_CALLBACK PostOperation;     // callback function called after each handle operation
    KSPIN_LOCK Lock;         // lock object used for synchronization
} OB_CALLBACK_ENTRY, * POB_CALLBACK_ENTRY;

typedef struct _OB_CALLBACK {
    USHORT Version;                           // usually 0x100
    USHORT OperationRegistrationCount;        // number of registered callbacks
    PVOID RegistrationContext;                // arbitrary data passed at registration time
    UNICODE_STRING AltitudeString;            // used to determine callbacks order
    OB_CALLBACK_ENTRY EntryItems[1]; // array of OperationRegistrationCount items
    WCHAR AltitudeBuffer[1];                  // is AltitudeString.MaximumLength bytes long, and pointed by AltitudeString.Buffer
} OB_CALLBACK, * POB_CALLBACK;
