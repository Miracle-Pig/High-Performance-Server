#include "../src/config.h"
#include "../src/log.h"
#include <yaml-cpp/yaml.h>
#include <iostream>

//# 1) 首先通过参数名称、参数值、参数描述创建并添加配置参数子类对象到所有配置项
HPS::ConfigVar<int>::ptr g_int_value_config =
    HPS::Config::Lookup("system.port", (int)8080, "system port");

HPS::ConfigVar<float>::ptr g_int_valuex_config =
    HPS::Config::Lookup("system.port", (float)8080, "system port");

HPS::ConfigVar<float>::ptr g_float_value_config =
    HPS::Config::Lookup("system.value", (float)10.2f, "system value");

HPS::ConfigVar<std::vector<int>>::ptr g_int_vec_value_config =
    HPS::Config::Lookup("system.int_vec", std::vector<int>{1, 2}, "system int vec");

HPS::ConfigVar<std::list<int>>::ptr g_int_list_value_config =
    HPS::Config::Lookup("system.int_list", std::list<int>{1, 2}, "system int list");

HPS::ConfigVar<std::set<int>>::ptr g_int_set_value_config =
    HPS::Config::Lookup("system.int_set", std::set<int>{1, 2}, "system int set");

HPS::ConfigVar<std::unordered_set<int>>::ptr g_int_uset_value_config =
    HPS::Config::Lookup("system.int_uset", std::unordered_set<int>{1, 2}, "system int uset");

HPS::ConfigVar<std::map<std::string, int>>::ptr g_str_int_map_value_config =
    HPS::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k", 2}}, "system str int map");

HPS::ConfigVar<std::unordered_map<std::string, int>>::ptr g_str_int_umap_value_config =
    HPS::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k", 2}}, "system str int map");

void test_yaml()
{

    YAML::Node root = YAML::LoadFile("/home/miracle/Zone/workspace/HPS/bin/conf/test.yml");
    // print_yaml(root, 0);
    // LOG_INFO(LOG_ROOT()) << root.Scalar();

    LOG_INFO(LOG_ROOT()) << root["test"].IsDefined();
    LOG_INFO(LOG_ROOT()) << root["logs"].IsDefined();
    LOG_INFO(LOG_ROOT()) << root;
}

void test_config()
{
    LOG_INFO(LOG_ROOT()) << "before: " << g_int_value_config->getValue();
    LOG_INFO(LOG_ROOT()) << "before: " << g_float_value_config->toString();

#define XX(g_var, name, prefix)                                                   \
    {                                                                             \
        auto &v = g_var->getValue();                                              \
        for (auto &i : v)                                                         \
        {                                                                         \
            LOG_INFO(LOG_ROOT()) << #prefix " " #name ": " << i;                  \
        }                                                                         \
        LOG_INFO(LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->toString(); \
    }

#define XX_M(g_var, name, prefix)                                                 \
    {                                                                             \
        auto &v = g_var->getValue();                                              \
        for (auto &i : v)                                                         \
        {                                                                         \
            LOG_INFO(LOG_ROOT()) << #prefix " " #name ": {"                       \
                                 << i.first << " - " << i.second << "}";          \
        }                                                                         \
        LOG_INFO(LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->toString(); \
    }

    XX(g_int_vec_value_config, int_vec, before);
    XX(g_int_list_value_config, int_list, before);
    XX(g_int_set_value_config, int_set, before);
    XX(g_int_uset_value_config, int_uset, before);
    XX_M(g_str_int_map_value_config, str_int_map, before);
    XX_M(g_str_int_umap_value_config, str_int_umap, before);
    //# 2) 从.yml文件中导入根yaml node
    YAML::Node root = YAML::LoadFile("/home/miracle/Zone/workspace/HPS/bin/conf/test.yml");
    HPS::Config::LoadFromYaml(root);

    LOG_INFO(LOG_ROOT()) << "after: " << g_int_value_config->getValue();
    LOG_INFO(LOG_ROOT()) << "after: " << g_float_value_config->toString();

    XX(g_int_vec_value_config, int_vec, after);
    XX(g_int_list_value_config, int_list, after);
    XX(g_int_set_value_config, int_set, after);
    XX(g_int_uset_value_config, int_uset, after);
    XX_M(g_str_int_map_value_config, str_int_map, after);
    XX_M(g_str_int_umap_value_config, str_int_umap, after);
}

void print_yaml(const YAML::Node &node, int level)
{
    if (node.IsScalar())
    {
        LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ')
                             << node.Scalar() << " - " << node.Type() << " - " << level;
    }
    else if (node.IsNull())
    {
        LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ')
                             << "NULL - " << node.Type() << " - " << level;
    }
    else if (node.IsMap())
    {
        for (auto it = node.begin();
             it != node.end(); ++it)
        {
            LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ')
                                 << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    }
    else if (node.IsSequence())
    {
        for (size_t i = 0; i < node.size(); ++i)
        {
            LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ')
                                 << i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level + 1);
        }
    }
}



class Person
{
public:
    Person(){};
    std::string m_name;
    int m_age = 0;
    bool m_sex = 0;

    std::string toString() const
    {
        std::stringstream ss;
        ss << "[Person name=" << m_name
           << " age=" << m_age
           << " sex=" << m_sex
           << "]";
        return ss.str();
    }

    bool operator==(const Person &oth) const
    {
        return m_name == oth.m_name && m_age == oth.m_age && m_sex == oth.m_sex;
    }
};

namespace HPS
{

    template <>
    class LexicalCast<std::string, Person>
    {
    public:
        Person operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            Person p;
            p.m_name = node["name"].as<std::string>();
            p.m_age = node["age"].as<int>();
            p.m_sex = node["sex"].as<bool>();
            return p;
        }
    };

    template <>
    class LexicalCast<Person, std::string>
    {
    public:
        std::string operator()(const Person &p)
        {
            YAML::Node node;
            node["name"] = p.m_name;
            node["age"] = p.m_age;
            node["sex"] = p.m_sex;
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

}

HPS::ConfigVar<Person>::ptr g_person =
    HPS::Config::Lookup("class.person", Person(), "system person");

HPS::ConfigVar<std::map<std::string, Person>>::ptr g_person_map =
    HPS::Config::Lookup("class.map", std::map<std::string, Person>(), "system person");

HPS::ConfigVar<std::map<std::string, std::vector<Person>>>::ptr g_person_vec_map =
    HPS::Config::Lookup("class.vec_map", std::map<std::string, std::vector<Person>>(), "system person");

void test_class()
{
    LOG_INFO(LOG_ROOT()) << "before: " << g_person->getValue().toString() << " - " << g_person->toString();

#define XX_PM(g_var, prefix)                                                                   \
    {                                                                                          \
        auto m = g_person_map->getValue();                                                     \
        for (auto &i : m)                                                                      \
        {                                                                                      \
            LOG_INFO(LOG_ROOT()) << prefix << ": " << i.first << " - " << i.second.toString(); \
        }                                                                                      \
        LOG_INFO(LOG_ROOT()) << prefix << ": size=" << m.size();                               \
    }

    g_person->addListener([](const Person &old_value, const Person &new_value)
                          { LOG_INFO(LOG_ROOT()) << "old_value=" << old_value.toString()
                                                 << " new_value=" << new_value.toString(); });

    XX_PM(g_person_map, "class.map before");
    LOG_INFO(LOG_ROOT()) << "before: " << g_person_vec_map->toString();

    YAML::Node root = YAML::LoadFile("/home/miracle/Zone/workspace/HPS/bin/conf/test.yml");
    HPS::Config::LoadFromYaml(root);

    LOG_INFO(LOG_ROOT()) << "after: " << g_person->getValue().toString() << " - " << g_person->toString();
    XX_PM(g_person_map, "class.map after");
    LOG_INFO(LOG_ROOT()) << "after: " << g_person_vec_map->toString();
}

void test_log() {
    static HPS::Logger::ptr system_log = LOG_NAME("system");
    LOG_INFO(system_log) << "hello system" << std::endl;
    std::cout << HPS::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    YAML::Node root = YAML::LoadFile("/home/miracle/Zone/workspace/HPS/bin/conf/log.yml");
    HPS::Config::LoadFromYaml(root);
    std::cout << "=============" << std::endl;
    std::cout << HPS::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    std::cout << "=============" << std::endl;
    std::cout << root << std::endl;
    LOG_INFO(system_log) << "hello system" << std::endl;

    system_log->setFormatter("%d - %m%n");
    LOG_INFO(system_log) << "hello system" << std::endl;
}

// void test_loadconf() {
//     HPS::Config::LoadFromConfDir("conf");
// }

int main(int argc, char **argv)
{
    // test_config();
    // test_yaml();
    // test_class();
    test_log();
    //  HPS::EnvMgr::GetInstance()->init(argc, argv);
    //  test_loadconf();
    //  std::cout << " ==== " << std::endl;
    //  sleep(10);
    //  test_loadconf();
    //  return 0;
    //  HPS::Config::Visit([](HPS::ConfigVarBase::ptr var) {
    //      LOG_INFO(LOG_ROOT()) << "name=" << var->getName()
    //                  << " description=" << var->getDescription()
    //                  << " typename=" << var->getTypeName()
    //                  << " value=" << var->toString();
    //  });

    return 0;
}
