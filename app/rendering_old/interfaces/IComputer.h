#pragma once

class IComputer
{
public:
  virtual ~IComputer() = default;

  virtual void initialize() = 0;
  virtual void execute() = 0;
};
