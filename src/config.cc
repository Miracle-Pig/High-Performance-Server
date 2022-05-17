#include "config.h"
#include "env.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HPS
{

    static HPS::Logger::ptr g_logger = LOG_NAME("system");

    ConfigVarBase::ptr Config::LookupBase(const std::string &name)
    {
        // RWMutexType::ReadLock lock(GetMutex());
        auto it = GetDatas().find(name);
        return it == GetDatas().end() ? nullptr : it->second;
    }

    //"A.B", 10
    // A:
    //  B: 10
    //  C: str
    //# 3) 从yaml root node开始，递归(因为有map类型)提取.yml文件中的所有yaml node存储到list容器中
    static void ListAllMember(const std::string &prefix,
                              const YAML::Node &node,
                              std::list<std::pair<std::string, const YAML::Node>> &output)
    {
        if (prefix.find_first_not_of("abcdefghikjlmnopqrstuvwxyzABCDEFGHIKJLMNOPQRSTUVWXYZ._012345678") != std::string::npos)
        {
            LOG_ERROR(g_logger) << "Config invalid name: " << prefix << " : " << node;
            return;
        }
        output.push_back(std::make_pair(prefix, node));
        if (node.IsMap())
        {
            for (auto it = node.begin();
                 it != node.end(); ++it)
            {
                ListAllMember(prefix.empty() ? it->first.Scalar()
                                             : prefix + "." + it->first.Scalar(),
                              it->second, output);
            }
        }
    }
    //# 4) 遍历list容器，将各yaml node转为配置参数
    void Config::LoadFromYaml(const YAML::Node &root)
    {
        std::list<std::pair<std::string, const YAML::Node>> all_nodes;
        ListAllMember("", root, all_nodes);
        //# 4.1 遍历所有yaml node
        for (auto &i : all_nodes)
        {
            std::string key = i.first;
            if (key.empty())
            {
                continue;
            }

            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            //# 4.2 通过配置参数名查找到配置参数基类
            ConfigVarBase::ptr var = LookupBase(key);
            //# 4.3 若yaml node为scalar直接转为yaml node；若yaml node非scalar则通过yaml字符串转为yaml node
            if (var)
            {
                if (i.second.IsScalar())
                {
                    var->fromString(i.second.Scalar());
                }
                else
                {
                    std::stringstream ss;
                    ss << i.second;
                    var->fromString(ss.str());
                }
            }
        }
    }

    static std::map<std::string, uint64_t> s_file2modifytime;
    // static HPS::Mutex s_mutex;

    void Config::LoadFromConfDir(const std::string &path, bool force)
    {
        std::string absoulte_path = HPS::EnvMgr::GetInstance()->getAbsolutePath(path);
        std::vector<std::string> files;
        FSUtil::ListAllFile(files, absoulte_path, ".yml");

        for (auto &i : files)
        {
            {
                //! 文件状态结构体
                struct stat st;
                //! 返回文件属性信息
                lstat(i.c_str(), &st);
                // HPS::Mutex::Lock lock(s_mutex);
                //! 如果文件没有被修改过
                //! 文件最后一次修改时间st_mtime
                if (!force && s_file2modifytime[i] == (uint64_t)st.st_mtime)
                {
                    continue;
                }
                s_file2modifytime[i] = st.st_mtime;
            }
            try
            {
                YAML::Node root = YAML::LoadFile(i);
                LoadFromYaml(root);
                LOG_INFO(g_logger) << "LoadConfFile file="
                                         << i << " ok";
            }
            catch (...)
            {
                LOG_ERROR(g_logger) << "LoadConfFile file="
                                          << i << " failed";
            }
        }
    }

    void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb)
    {
        // RWMutexType::ReadLock lock(GetMutex());
        ConfigVarMap &m = GetDatas();
        for (auto it = m.begin();
             it != m.end(); ++it)
        {
            cb(it->second);
        }
    }

}
