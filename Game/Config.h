#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;
#include "../Models/Project_path.h"

class Config
{
  public:
    Config()  //это своего рода "интерфейс" для доступа к настройкам, 
    //предоставляющий более простой, понятный и удобный способ
    {
        reload();
    }

    void reload() //обеспечивает механизм для загрузки и обновления настроек из файла.
    {
        ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }

    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
