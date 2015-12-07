
class PassList
{
private:
  friend class PassListParser;
  
  std::vector<Pass *> passes;
  std::vector<std::vector<std::string>> pass_args;
  
public:
  PassList(const std::string &filename);
  
  void run(DesignState &ds) const;
};
