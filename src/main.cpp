#include "config.h"
#include "launcher.h"
#include "tui_app.h"

int main()
{
    Config cfg = loadConfig();

    std::string cmd = runTUI(cfg);

    if (!cmd.empty())
        launchOrExec(cmd);

    return 0;
}
