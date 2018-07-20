#include "crow.h"

#include <sstream>
#include "lsblk.h"

class ExampleLogHandler : public crow::ILogHandler {
    public:
        void log(std::string /*message*/, crow::LogLevel /*level*/) override {
//            cerr << "ExampleLogHandler -> " << message;
        }
};


int main()
{
    crow::SimpleApp app;


    CROW_ROUTE(app, "/lsblk")
    ([]{
	crow::json::wvalue y;
	list_blk(y);
        return crow::json::dump(y);
    });


    app.loglevel(crow::LogLevel::INFO);

    app.port(18080)
        .multithreaded()
        .run();
}
