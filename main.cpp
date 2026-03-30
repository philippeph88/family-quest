#include "httplib.h"
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

struct User { std::string name; int points; };
struct Quest { int id; std::string desc; int xp; }; // done entfernt

std::map<std::string, User> family = {
    {"Mami", {"Mami", 0}}, {"Papi", {"Papi", 0}},
    {"Nathalie", {"Nathalie", 0}}, {"Philippe", {"Philippe", 0}}
};

std::vector<Quest> quests = {
    {1, "Spuelmaschine ausraeumen 🍽️", 50},
    {2, "Muell rausbringen 🗑️", 20},
    {3, "Zimmer aufraeumen ✨", 100},
    {4, "Tisch decken 🍴", 15},
    {5, "Philippe hat Sachen von der Treppe mitgenommen 📦", 30}
};

std::mutex dataMutex;

void save_to_file() {
    std::ofstream file("speicher.txt");
    if (file.is_open()) {
        for (auto const& [name, user] : family) file << "U:" << name << ":" << user.points << "\n";
    }
}

void load_from_file() {
    std::ifstream file("speicher.txt");
    std::string line;
    while (std::getline(file, line)) {
        try {
            if (line.substr(0, 2) == "U:") {
                size_t p = line.find(':', 2);
                std::string n = line.substr(2, p - 2);
                if (family.count(n)) family[n].points = std::stoi(line.substr(p + 1));
            }
        } catch(...) {}
    }
}

std::string get_html() {
    return R"(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Family Cloud</title>
<style>
    body{font-family:sans-serif;background:#a1c4fd;background-image:linear-gradient(120deg,#a1c4fd 0%,#c2e9fb 100%);min-height:100vh;text-align:center;padding:20px;}
    .card{background:white;margin:10px auto;padding:20px;border-radius:20px;max-width:500px;box-shadow:0 10px 20px rgba(0,0,0,0.1);border:4px solid #f093fb;}
    button{background:#f093fb;color:white;border:none;padding:10px 15px;border-radius:50px;cursor:pointer;margin:5px;font-weight:bold;}
    .xp{color:#f093fb;font-weight:bold;}
</style></head>
<body>
    <h1>☁️ Family Quest Cloud ☁️</h1>
    <div id='status'></div><hr><div id='quests'></div>
    <script>
        function update(){
            fetch('/data').then(r=>r.json()).then(data=>{
                let s='<h2>🏆 Highscore</h2>';
                for(let n in data.users) s+='<div class="card">'+n+'<br><span class="xp">'+data.users[n]+' XP</span></div>';
                document.getElementById('status').innerHTML=s;
                let qh='<h2>📜 Missionen</h2>';
                data.quests.forEach(q=>{
                    qh+='<div class="card"><h3>'+q.desc+'</h3><p class="xp">'+q.xp+' XP</p>';
                    if(q.desc.includes('Treppe')){ qh+='<button onclick=\"doit('+q.id+',\'Philippe\')\">Philippe war\'s!</button>'; }
                    else { ['Mami','Papi','Nathalie','Philippe'].forEach(u=>{ qh+='<button onclick=\"doit('+q.id+',\''+u+'\')\">'+u+'</button>'; }); }
                    qh+='</div>';
                });
                document.getElementById('quests').innerHTML=qh;
            });
        }
        function doit(id,user){ fetch('/do/'+id+'/'+user).then(()=>update()); }
        setInterval(update, 3000); update();
    </script>
</body></html>
)";
}

int main() {
    load_from_file();
    httplib::Server svr;
    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 8080;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) { res.set_content(get_html(), "text/html"); });
    svr.Get("/data", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::string j = "{\"users\":{";
        for(auto const& [n, u] : family) j += "\"" + n + "\":" + std::to_string(u.points) + ",";
        if(j.back()==',') j.pop_back();
        j += "},\"quests\":[";
        for(auto const& q : quests) j += "{\"id\":" + std::to_string(q.id) + ",\"desc\":\"" + q.desc + "\",\"xp\":" + std::to_string(q.xp) + "},";
        if(j.back()==',') j.pop_back();
        j += "]}";
        res.set_content(j, "application/json");
    });
    svr.Get("/do/:id/:user", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(dataMutex);
        try {
            int id = std::stoi(req.path_params.at("id"));
            std::string user = req.path_params.at("user");
            for(auto& q : quests){ if(q.id == id){ family[user].points += q.xp; save_to_file(); } }
        } catch(...) {}
        res.set_content("ok", "text/plain");
    });
    svr.listen("0.0.0.0", port);
    return 0;
}
