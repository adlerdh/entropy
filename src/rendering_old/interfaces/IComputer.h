#ifndef I_COMPUTER_H
#define I_COMPUTER_H

class IComputer
{
public:
  virtual ~IComputer() = default;

  virtual void initialize() = 0;
  virtual void execute() = 0;
};

#endif // I_COMPUTER_H
