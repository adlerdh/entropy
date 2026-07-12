#pragma once

class ComputerBase
{
public:
  ComputerBase() = default;
  virtual ~ComputerBase() = default;

  virtual void initialize() = 0;
  virtual void execute() = 0;
};
