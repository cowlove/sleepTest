#include "jimlib.h"
#include "serialLog.h"
#include "espNowMux.h"
#include "reliableStream.h"
#ifndef CSIM
#include "rom/uart.h"
#endif

JStuff j;
CLI_VARIABLE_FLOAT(x, 1);

class BSleepNode {
    std::map<string,int> macsSeen;
public: 
    string mac = getMacAddress().c_str();
    ReliableStreamESPNow espNow = ReliableStreamESPNow("BSLEEP", true);
    void run() { 
        string in = espNow.read();
        if (in != "") { 
            OUT("<<<< %s", in.c_str());
            macsSeen[in]++;
        }
        if (j.secTick(1)) { 
            OUT(">>>> %s", mac.c_str());
            espNow.write(mac);
        }
        if (millis() > 5000) { 
            float sleepSec = ESPNowMux::Instance->bwakeup.getSleepSec();
            OUT("macs seen %d sleep %.2f", macsSeen.size(), sleepSec);
            for(auto m : macsSeen) {
                OUT("%12s %d", m.first.c_str(), m.second);
            }
            if (sleepSec < 0) 
                sleepSec = 60;
            deepSleep(1000 * sleepSec);
        }
    }
};

void setup() {
    j.begin();
    j.jw.enabled = false;
    //j.cli.on("RESET", [](){ ESP.restart(); });
}

int seedS3LED = 21;
BSleepNode client;
void loop() {
    j.run();
    client.run();
    delay(1);
}

#ifdef CSIM
class SketchCsim : public Csim_Module {
    BSleepNode client1;

    public:
    void setup() { 
        HTTPClient::csim_onPOST("http://.*/log", 
            [](const char *url, const char *hdr, const char *data, string &result) {
 	            return 200; });
        client.mac = "FOOMAC"; 
        ESPNOW_sendHandler = new ESPNOW_csimOneProg();
        csim_flags.OneProg = true;
     }
    string dummy;
    void parseArg(char **&a, char **la) override { if (strcmp(*a, "--dummy") == 0) dummy = *(++a); }
    void loop() override {
        client1.run();
    }
} sketchCsim;
#endif
 
