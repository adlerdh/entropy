#ifndef COMPUTER_BASE_H
#define COMPUTER_BASE_H

class ComputerBase
{
public:
  ComputerBase() = default;
  virtual ~ComputerBase() = default;

  virtual void initialize() = 0;
  virtual void execute() = 0;
};

#endif // COMPUTER_BASE_H
