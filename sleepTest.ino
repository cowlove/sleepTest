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
    SPIFFSVariable<int> wakeups = SPIFFSVariable<int>("/BSlpWakes", 0);
    SPIFFSVariable<int> msAwake = SPIFFSVariable<int>("/BStmAwake", 0);
    DeepSleepElapsedTimer dsTime = DeepSleepElapsedTimer("/BS_dset");

    std::map<string,int> macsSeen;
    int pktsSeen = 0;
    bool firstLoop = true;
public: 
    string mac = getMacAddress().c_str();
    ReliableStreamESPNow espNow = ReliableStreamESPNow("BSLEEP", true);
    void run() { 
        if (firstLoop == true) { 
            firstLoop = false;
            if (getResetReason() != 5) {
                wakeups = 0;
                msAwake = 0;
            }
        }
        string in = espNow.read();
        if (in != "") { 
            OUT("<<<< %s", in.c_str());
            macsSeen[in]++;
            pktsSeen++;
        }
        if (j.secTick(1)) { 
            OUT(">>>> %s", mac.c_str());
            espNow.write(mac);
        }
        if (millis() > 5000) { 
            msAwake = msAwake + millis();
            wakeups = wakeups + 1;
            float sleepSec = ESPNowMux::Instance->bwakeup.getSleepSec();
            OUT(sfmt("wakes %d wakeSec %.2f realSec %.2f macs %d pkts %d sleep %.2f ", 
                wakeups.read(), msAwake / 1000.0, dsTime.millis() / 1000.0,  
                macsSeen.size(), pktsSeen, sleepSec) + ESPNowMux::Instance->bwakeup.getStats());
            for(auto m : macsSeen) {
                OUT("%12s %d", m.first.c_str(), m.second);
            }
            if (macsSeen.size() > 0) {
                dsTime.reset();
                wakeups = 0;
                msAwake = 0;
            }
            if (sleepSec < 0) 
                sleepSec = 0;
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
 
