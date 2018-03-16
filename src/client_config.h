#pragma once

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "namespaces.h"
#include "util.h"

namespace ouinet {

class ClientConfig {
    using Path = boost::filesystem::path;

public:
    ClientConfig();

    // Throws on error
    ClientConfig(int argc, char* argv[]);

    ClientConfig(const ClientConfig&) = default;
    ClientConfig& operator=(const ClientConfig&) = default;

    const Path& repo_root() const {
        return _repo_root;
    }

    const Endpoint& injector_endpoint() const {
        return _injector_ep;
    }

    bool set_injector_endpoint(const std::string& ep);

    const asio::ip::tcp::endpoint& local_endpoint() const {
        return _local_ep;
    }

    const std::string& ipns() const {
        return _ipns;
    }

    boost::posix_time::time_duration max_cached_age() const {
        return _max_cached_age;
    }

private:
    Path _repo_root;
    Path _ouinet_conf_file = "ouinet-client.conf";
    asio::ip::tcp::endpoint _local_ep;
    Endpoint _injector_ep;
    std::string _ipns;

    boost::posix_time::time_duration _max_cached_age
        = boost::posix_time::hours(7*24);  // one week
};

inline
ClientConfig::ClientConfig() { }

inline
ClientConfig::ClientConfig(int argc, char* argv[])
{
    using namespace std;
    namespace po = boost::program_options;
    namespace fs = boost::filesystem;

    po::options_description desc("\nOptions");

    desc.add_options()
        ("help", "Produce this help message")
        ("repo", po::value<string>(), "Path to the repository root")
        ("listen-on-tcp", po::value<string>(), "IP:PORT endpoint on which we'll listen")
        ("injector-ep"
         , po::value<string>()
         , "Injector's endpoint (either <IP>:<PORT> or <GNUnet's ID>:<GNUnet's PORT>")
        ("injector-ipns"
         , po::value<string>()->default_value("")
         , "IPNS of the injector's database")
        ("max-cached-age"
         , po::value<int>()->default_value(_max_cached_age.total_seconds())
         , "Discard cached content older than this many seconds (0: discard all; -1: discard none)")
        ("open-file-limit"
         , po::value<unsigned int>()
         , "To increase the maximum number of open files")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (!vm.count("repo")) {
        throw std::runtime_error(
                util::str("The 'repo' argument is missing\n", desc, "\n"));
    }

    _repo_root = fs::path(vm["repo"].as<string>());

    if (!fs::exists(_repo_root)) {
        throw std::runtime_error(
                util::str("Directory ", _repo_root, " does not exist.\n"
                         , desc, "\n"));
    }

    if (!fs::is_directory(_repo_root)) {
        throw std::runtime_error(
                util::str("The path ", _repo_root, " is not a directory.\n"
                         , desc, "\n"));
    }

    fs::path ouinet_conf_path = _repo_root/_ouinet_conf_file;

    if (!fs::is_regular_file(ouinet_conf_path)) {
        throw std::runtime_error(
                util::str("The path ", _repo_root, " does not contain the "
                         , _ouinet_conf_file, " configuration file.\n"
                         , desc));
    }

    ifstream ouinet_conf(ouinet_conf_path.native());

    po::store(po::parse_config_file(ouinet_conf, desc), vm);
    po::notify(vm);

    if (vm.count("open-file-limit")) {
        increase_open_file_limit(vm["open-file-limit"].as<unsigned int>());
    }

    if (vm.count("max-cached-age")) {
        _max_cached_age = boost::posix_time::seconds(vm["max-cached-age"].as<int>());
    }

    if (!vm.count("listen-on-tcp")) {
        throw std::runtime_error(
                util::str( "The parameter 'listen-on-tcp' is missing.\n"
                         , desc, "\n"));
    }

    if (!vm.count("injector-ep")) {
        throw std::runtime_error(
                util::str( "The parameter 'injector-ep' is missing.\n"
                         , desc, "\n"));
    }

    _local_ep = util::parse_endpoint(vm["listen-on-tcp"].as<string>());

    if (!set_injector_endpoint(vm["injector-ep"].as<string>())) {
        throw std::runtime_error(
                "Failed to parse endpoint \""
                + vm["injector-ep"].as<string>() + "\"");
    }


    if (vm.count("injector-ipns")) {
        _ipns = vm["injector-ipns"].as<string>();
    }
}

inline
bool ClientConfig::set_injector_endpoint(const std::string& ep)
{
    auto opt = parse_endpoint(ep);
    if (!opt) return false;
    _injector_ep = *opt;
    return true;
}

} // ouinet namespace
