
#include <string>
#include <vector>
#include <map>

class DesignState;

class Pass
{
private:
  friend class PassList;
  friend class PassListParser; // FIXME
  
  std::string m_name;
  
  // FIXME lookup
  static std::map<std::string, Pass *> passes;
  
  virtual void run(DesignState &ds, const std::vector<std::string> &args) const = 0;
  
public:
  const std::string &name() const { return m_name; }
  
  static void run(DesignState &ds, std::string pass_name, std::vector<std::string> args);
  static void run(DesignState &ds, std::string pass_name)
  {
    run(ds, pass_name, {});
  }
  
  Pass(const std::string &n);
  virtual ~Pass();
};
