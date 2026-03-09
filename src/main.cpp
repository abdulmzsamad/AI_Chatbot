#include "gui.h"
#include "api_client.h"

int main(int argc, char* argv[]) {
    ApiClient::init();    // initialize persistent CURL connection
    GUI app;
    app.run();
    ApiClient::cleanup(); // clean up on exit
    return 0;
}