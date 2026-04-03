#include "httplib.h"
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>

struct User { std::string name; int points; };
struct Quest { int id; std::string desc; int xp; };

std::map<std::string, User> family = {
    {"Mami", {"Mami", 0}}, {"Papi", {"Papi", 0}},
    {"Nathalie", {"Nathalie", 0}}, {"Philippe", {"Philippe", 0}}
};

std::vector<Quest> quests = {
    {1, "Spuelmaschine ausraeumen 🍽️", 50},
    {2, "Muell rausbringen 🗑️", 20},
    {3, "Zimmer aufraeumen ✨", 40}, 
    {4, "Tisch decken 🍴", 15},
    {5, "Philippe hat Sachen von der Treppe mitgenommen 📦", 30}
};

std::mutex dataMutex;
const std::string FILE_NAME = "speicher.txt";

// Speichert den aktuellen Stand
void save_to_file() {
    std::ofstream file(FILE_NAME, std::ios::trunc);
    if (file.is_open()) {
        for (auto const& [name, user] : family) {
            file << name << ":" << user.points << "\n";
        }
        file.close();
        std::cout << "Daten erfolgreich gesichert." << std::endl;
    }
}

// Laedt den Stand
void load_from_file() {
    std::ifstream file(FILE_NAME);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        size_t p = line.find(':');
        if (p != std::string::npos) {
            std::string n = line.substr(0, p);
            try {
                int pts = std::stoi(line.substr(p + 1));
                if (family.count(n)) {
                    family[n].points = pts;
                }
            } catch(...) {}
        }
    }
    file.close();
    std::cout << "Daten erfolgreich geladen." << std::endl;
}

std::string get_html() {
    return R"(
<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Family Quest Cloud</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
    body{font-family:sans-serif;background:#a1c4fd;background-image:linear-gradient(120deg,#a1c4fd 0%,#c2e9fb 100%);min-height:100vh;text-align:center;padding:20px;margin:0;}
    h1{color:white;text-shadow:2px 2px 4px rgba(0,0,0,0.2);}
    .card{background:white;margin:15px auto;padding:20px;border-radius:20px;max-width:500px;box-shadow:0 10px 20px rgba(0,0,0,0.1);border:4px solid #f093fb;transition:0.3s;}
    .card:hover{transform:scale(1.02);}
    button{background:#f093fb;color:white;border:none;padding:12px 20px;border-radius:50px;cursor:pointer;margin:5px;font-weight:bold;box-shadow:0 4px 6px rgba(0,0,0,0.1);}
    button:active{transform:translateY(2px);box-shadow:none;}
    .xp-badge{background:#f093fb;color:white;padding:5px 10px;border-radius:10px;font-size:0.9em;margin-left:10px;}
    .points{font-size:1.5em;color:#f093fb;font-weight:bold;}
    hr{border:0;height:1px;background-image:linear-gradient(to right,rgba(0,0,0,0),rgba(255,255,255,0.75),rgba(0,0,0,0));margin:30px 0;}
</style></head>
<body>
    <h1>☁️ Family Quest Cloud ☁️</h1>
    <div id='status'></div><hr><h2>📜 Verfügbare Missionen</h2><div id='quests'></div>
    <script>
        function update(){
            fetch('/data').then(r=>r.json()).then(data=>{
                let s='<h2>🏆 Highscore</h2>';
                for(let n in data.users) {
                    s+='<div class="card"><strong>'+n+'</strong><br><span class="points">'+data.users[n]+' XP</span></div>';
                }
                document.getElementById('status').innerHTML=s;
                let qh='';
                data.quests.forEach(q=>{
                    qh+='<div class="card"><h3>'+q.desc+' <span class="xp-badge">'+q.xp+' XP</span></h3>';
                    if(q.desc.includes('Treppe')){ 
                        qh+='<button onclick=\"doit('+q.id+',\'Philippe\')\">Philippe war\'s!</button>'; 
                    } else { 
                        ['Mami','Papi','Nathalie','Philippe'].forEach(u=>{ 
                            qh+='<button onclick=\"doit('+q.id+',\''+u+'\')\">'+u+'</button>'; 
                        }); 
                    }
                    qh+='</div>';
                });
                document.getElementById('quests').innerHTML=qh;
            });
        }
        function doit(id,user){ fetch('/do/'+id+'/'+user).then(()=>update()); }
        setInterval(update, 4000); update();
    </script>
</body></html>
)";
}

int main() {
    load_from_file();
    httplib::Server svr;
    
    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 8080;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) { 
        res.set_content(get_html(), "text/html"); 
    });

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
            if (family.count(user)) {
                for(auto& q : quests) {
                    if(q.id == id) {
                        family[user].points += q.xp;
                        save_to_file();
                        break;
                    }
                }
            }
        } catch(...) {}
        res.set_content("ok", "text/plain");
    });

    std::cout << "Server laeuft auf Port " << port << std::endl;
    svr.listen("0.0.0.0", port);
    return 0;
}
