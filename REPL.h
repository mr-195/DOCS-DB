#include<bits/stdc++.h>
#include<sys/shm.h>
#include"database.h"
class REPL:Database{
    public:
    bool GET(string&,string&);
    bool SET(string&, string&);
    bool DELETE(string&);
};