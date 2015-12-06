
class PassList
{
private:
  friend class PassListParser;
  
  std::vector<Pass *> passes;
  std::vector<std::vector<std::string>> pass_args;
  
public:
  void run(DesignState &ds) const;
};
