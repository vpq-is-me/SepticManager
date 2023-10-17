# SepticManager
managing interaction with septic automation

//**********************************************************************
for using jansson.h not forget install jansson library. 
e.i. Dowload jansson project, and install. For futher info read README in janson library. 
And not forget add -ljansson flag in project build properties in linker other options
//**********************************************************************
for succesful compiling of sqlite3 add link libraries: 
pthread
dl
to project build properties ->link libraries
//**********************************************************************

request from web client to this:

{"tag": 1/2/3 (TAG_REQSNAP/TAG_CMDRESETALARM/TAG_DB_REQUEST****}

if tag==TAG_REQSNAP there is explition parameter of requested data:
"val": "act_alarm"/"pend_alarm"/"msg_timer"}
i.e. {"tag": 1, "val": "msg_timer"}
