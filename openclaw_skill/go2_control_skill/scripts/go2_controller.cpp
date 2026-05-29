#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <unitree/idl/go2/SportModeState_.hpp>
#include <unitree/robot/channel/channel_factory.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/robot/go2/robot_state/robot_state_client.hpp>
#include <unitree/robot/go2/sport/sport_client.hpp>

namespace {

using unitree::robot::ChannelFactory;
using unitree::robot::ChannelSubscriber;
using unitree::robot::ChannelSubscriberPtr;
using unitree::robot::go2::RobotStateClient;
using unitree::robot::go2::ServiceState;
using unitree::robot::go2::SportClient;
using unitree_go::msg::dds_::SportModeState_;

constexpr const char* kTopicHighState = "rt/sportmodestate";
constexpr const char* kEarlyLogPath = "/home/radxa/.openclaw/workspace/skills/go2_control_skill/logs/go2_controller.early.log";

void AppendEarlyLog(const std::string& line) {
  int fd = ::open(kEarlyLogPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    return;
  }
  std::string payload = line + "\n";
  (void)!::write(fd, payload.c_str(), payload.size());
  ::close(fd);
}

struct ParsedArgs {
  std::string interface;
  std::string command;
  std::map<std::string, std::string> options;
};

std::optional<std::string> ReadOption(
    const std::map<std::string, std::string>& options,
    const std::string& key) {
  auto it = options.find(key);
  if (it == options.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool ReadBoolOption(
    const std::map<std::string, std::string>& options,
    const std::string& key,
    bool fallback) {
  auto value = ReadOption(options, key);
  if (!value.has_value()) {
    return fallback;
  }
  return *value == "1" || *value == "true" || *value == "yes";
}

float ReadFloatOption(
    const std::map<std::string, std::string>& options,
    const std::string& key,
    float fallback) {
  auto value = ReadOption(options, key);
  if (!value.has_value()) {
    return fallback;
  }
  return std::stof(*value);
}

ParsedArgs ParseArgs(int argc, char** argv) {
  ParsedArgs parsed;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--interface") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--interface requires a value");
      }
      parsed.interface = argv[++i];
      continue;
    }
    if (arg.rfind("--", 0) == 0) {
      if (i + 1 >= argc) {
        throw std::runtime_error(arg + " requires a value");
      }
      parsed.options[arg] = argv[++i];
      continue;
    }
    if (parsed.command.empty()) {
      parsed.command = arg;
      continue;
    }
    throw std::runtime_error("unexpected argument: " + arg);
  }

  if (parsed.interface.empty()) {
    throw std::runtime_error("missing --interface");
  }
  if (parsed.command.empty()) {
    throw std::runtime_error("missing command");
  }
  return parsed;
}

bool UseAutoInterfaceMode(const std::string& interface_name) {
  return interface_name == "auto" || interface_name == "default" || interface_name == "";
}

void PrintUsage(const char* argv0) {
  std::cerr
      << "Usage:\n"
      << "  " << argv0 << " --interface IFACE|auto status [--with-services true|false]\n"
      << "  " << argv0 << " --interface IFACE|auto stand-up\n"
      << "  " << argv0 << " --interface IFACE|auto stand-down\n"
      << "  " << argv0 << " --interface IFACE|auto sit\n"
      << "  " << argv0 << " --interface IFACE|auto rise-sit\n"
      << "  " << argv0 << " --interface IFACE|auto recover-stand\n"
      << "  " << argv0 << " --interface IFACE|auto balance-stand\n"
      << "  " << argv0 << " --interface IFACE|auto damp\n"
      << "  " << argv0 << " --interface IFACE|auto stop\n"
      << "  " << argv0 << " --interface IFACE|auto hello\n"
      << "  " << argv0 << " --interface IFACE|auto stretch\n"
      << "  " << argv0
      << " --interface IFACE|auto move --vx 0.2 --vy 0.0 --vyaw 0.0 --duration 1.5\n";
}

class Go2Bridge {
 public:
  explicit Go2Bridge(std::string interface) : interface_(std::move(interface)) {}

  ~Go2Bridge() {
    AppendEarlyLog("destructor: release");
    state_client_.reset();
    sport_client_.reset();
    try {
      ChannelFactory::Instance()->Release();
    } catch (...) {
    }
  }

  void Init(bool init_state_client) {
    if (UseAutoInterfaceMode(interface_)) {
      AppendEarlyLog("init: ChannelFactory::Init auto begin");
      ChannelFactory::Instance()->Init(0);
      AppendEarlyLog("init: ChannelFactory::Init auto done");
    } else {
      AppendEarlyLog("init: ChannelFactory::Init begin interface=" + interface_);
      ChannelFactory::Instance()->Init(0, interface_);
      AppendEarlyLog("init: ChannelFactory::Init done");
    }

    AppendEarlyLog("init: SportClient ctor begin");
    sport_client_ = std::make_unique<SportClient>();
    AppendEarlyLog("init: SportClient ctor done");
    sport_client_->SetTimeout(10.0f);
    AppendEarlyLog("init: SportClient::Init begin");
    sport_client_->Init();
    AppendEarlyLog("init: SportClient::Init done");

    if (init_state_client) {
      AppendEarlyLog("init: RobotStateClient ctor begin");
      state_client_ = std::make_unique<RobotStateClient>();
      AppendEarlyLog("init: RobotStateClient ctor done");
      state_client_->SetTimeout(10.0f);
      AppendEarlyLog("init: RobotStateClient::Init begin");
      state_client_->Init();
      AppendEarlyLog("init: RobotStateClient::Init done");
      state_client_ready_ = true;
    }

    AppendEarlyLog("init: subscriber begin");
    subscriber_.reset(new ChannelSubscriber<SportModeState_>(kTopicHighState));
    subscriber_->InitChannel(
        std::bind(&Go2Bridge::OnState, this, std::placeholders::_1), 1);
    AppendEarlyLog("init: subscriber done");
  }

  int Status(bool with_services) {
    AppendEarlyLog("status: begin");
    bool state_received = WaitForState(std::chrono::milliseconds(2000));

    std::cout << std::boolalpha;
    std::cout << "success: " << state_received << "\n";
    std::cout << "interface: " << interface_ << "\n";
    std::cout << "state_received: " << state_received << "\n";
    std::cout << "service_list_enabled: " << with_services << "\n";

    if (state_received) {
      SportModeState_ snapshot = LatestState();
      std::cout << std::fixed << std::setprecision(3);
      std::cout << "error_code: " << snapshot.error_code() << "\n";
      std::cout << "mode: " << static_cast<int>(snapshot.mode()) << "\n";
      std::cout << "gait_type: " << static_cast<int>(snapshot.gait_type()) << "\n";
      std::cout << "progress: " << snapshot.progress() << "\n";
      std::cout << "body_height: " << snapshot.body_height() << "\n";
      std::cout << "position: " << snapshot.position()[0] << " "
                << snapshot.position()[1] << " " << snapshot.position()[2] << "\n";
      std::cout << "velocity: " << snapshot.velocity()[0] << " "
                << snapshot.velocity()[1] << " " << snapshot.velocity()[2] << "\n";
      std::cout << "yaw_speed: " << snapshot.yaw_speed() << "\n";
      std::cout << "rpy: " << snapshot.imu_state().rpy()[0] << " "
                << snapshot.imu_state().rpy()[1] << " "
                << snapshot.imu_state().rpy()[2] << "\n";
    }

    if (!with_services) {
      std::cout << "services: skipped\n";
      return state_received ? 0 : 1;
    }

    if (!state_client_ready_ || !state_client_) {
      std::cout << "services: unavailable\n";
      return state_received ? 0 : 1;
    }

    AppendEarlyLog("status: ServiceList begin");
    std::vector<ServiceState> services;
    int service_ret = state_client_->ServiceList(services);
    AppendEarlyLog("status: ServiceList done ret=" + std::to_string(service_ret));
    std::cout << "service_list_ret: " << service_ret << "\n";
    std::cout << "services:\n";
    for (const auto& service : services) {
      std::cout << "- name=" << service.name << " status=" << service.status
                << " protect=" << service.protect << "\n";
    }

    return (state_received || service_ret == 0) ? 0 : 1;
  }

  int StandUp() { return CallSimple("stand-up", [&] { return sport_client_->StandUp(); }); }
  int StandDown() { return CallSimple("stand-down", [&] { return sport_client_->StandDown(); }); }
  int Sit() { return CallSimple("sit", [&] { return sport_client_->Sit(); }); }
  int RiseSit() { return CallSimple("rise-sit", [&] { return sport_client_->RiseSit(); }); }
  int RecoverStand() {
    return CallSimple("recover-stand", [&] { return sport_client_->RecoveryStand(); });
  }
  int BalanceStand() {
    return CallSimple("balance-stand", [&] { return sport_client_->BalanceStand(); });
  }
  int Damp() { return CallSimple("damp", [&] { return sport_client_->Damp(); }); }
  int Stop() { return CallSimple("stop", [&] { return sport_client_->StopMove(); }); }
  int Hello() { return CallSimple("hello", [&] { return sport_client_->Hello(); }); }
  int Stretch() { return CallSimple("stretch", [&] { return sport_client_->Stretch(); }); }

  int Move(float vx, float vy, float vyaw, float duration_s) {
    AppendEarlyLog("move: begin");
    int ret = sport_client_->Move(vx, vy, vyaw);
    std::cout << std::boolalpha;
    std::cout << "success: " << (ret == 0) << "\n";
    std::cout << "command: move\n";
    std::cout << "interface: " << interface_ << "\n";
    std::cout << "ret: " << ret << "\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "vx: " << vx << "\n";
    std::cout << "vy: " << vy << "\n";
    std::cout << "vyaw: " << vyaw << "\n";
    std::cout << "duration: " << duration_s << "\n";

    if (ret != 0) {
      return 1;
    }

    if (duration_s > 0.0f) {
      std::this_thread::sleep_for(std::chrono::duration<float>(duration_s));
      int stop_ret = sport_client_->StopMove();
      std::cout << "auto_stop_ret: " << stop_ret << "\n";
      if (stop_ret != 0) {
        return 1;
      }
    }
    return 0;
  }

 private:
  void OnState(const void* message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    latest_state_ = *static_cast<const SportModeState_*>(message);
    has_state_ = true;
  }

  bool WaitForState(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (has_state_) {
          return true;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
  }

  SportModeState_ LatestState() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return latest_state_;
  }

  template <typename Fn>
  int CallSimple(const std::string& command, Fn&& fn) {
    if (!sport_client_) {
      throw std::runtime_error("sport client is not initialized");
    }
    int ret = fn();
    std::cout << std::boolalpha;
    std::cout << "success: " << (ret == 0) << "\n";
    std::cout << "command: " << command << "\n";
    std::cout << "interface: " << interface_ << "\n";
    std::cout << "ret: " << ret << "\n";
    return ret == 0 ? 0 : 1;
  }

  std::string interface_;
  std::unique_ptr<SportClient> sport_client_;
  std::unique_ptr<RobotStateClient> state_client_;
  bool state_client_ready_ = false;
  ChannelSubscriberPtr<SportModeState_> subscriber_;
  std::mutex state_mutex_;
  SportModeState_ latest_state_{};
  bool has_state_ = false;
};

}  // namespace

int main(int argc, char** argv) {
  AppendEarlyLog("main: entered");
  try {
    ParsedArgs args = ParseArgs(argc, argv);
    AppendEarlyLog("main: parsed interface=" + args.interface + " command=" + args.command);
    bool with_services = ReadBoolOption(args.options, "--with-services", false);
    bool init_state_client = args.command != "status" || with_services;

    Go2Bridge bridge(args.interface);
    AppendEarlyLog("main: bridge constructed");
    bridge.Init(init_state_client);
    AppendEarlyLog("main: bridge initialized");

    if (args.command == "status") {
      return bridge.Status(with_services);
    }
    if (args.command == "stand-up") {
      return bridge.StandUp();
    }
    if (args.command == "stand-down") {
      return bridge.StandDown();
    }
    if (args.command == "sit") {
      return bridge.Sit();
    }
    if (args.command == "rise-sit") {
      return bridge.RiseSit();
    }
    if (args.command == "recover-stand") {
      return bridge.RecoverStand();
    }
    if (args.command == "balance-stand") {
      return bridge.BalanceStand();
    }
    if (args.command == "damp") {
      return bridge.Damp();
    }
    if (args.command == "stop") {
      return bridge.Stop();
    }
    if (args.command == "hello") {
      return bridge.Hello();
    }
    if (args.command == "stretch") {
      return bridge.Stretch();
    }
    if (args.command == "move") {
      float vx = ReadFloatOption(args.options, "--vx", 0.0f);
      float vy = ReadFloatOption(args.options, "--vy", 0.0f);
      float vyaw = ReadFloatOption(args.options, "--vyaw", 0.0f);
      float duration = ReadFloatOption(args.options, "--duration", 1.0f);
      return bridge.Move(vx, vy, vyaw, duration);
    }

    throw std::runtime_error("unknown command: " + args.command);
  } catch (const std::exception& ex) {
    AppendEarlyLog(std::string("main: exception: ") + ex.what());
    std::cerr << "ERROR: " << ex.what() << "\n";
    PrintUsage(argv[0]);
    return 2;
  }
}
