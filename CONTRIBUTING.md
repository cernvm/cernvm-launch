How to contribute to CernVM-Launch
===================================

When contributing, please take into consideration all the following information.

Coding style
============

Rule of thumb: when in doubt, look into the existing source code. Otherwise use the common sense.

- Use tabs instead of spaces at the beginning on a line. Use spaces if you want to align something "inside a line".
- Use lowerCamelCase for variables and UpperCamelCase for classes and global functions.
- Member variables shall begin with an underscore. Access them without using "this->".
- Member functions are called via "this->".
- Static member functions have to be prefixed with the class name.
- Opening brace is on the same line as the statement
- One line "if"s have their statement on the same line

E.g.:

    void Hypervisor::set(const std::string& version) {
        if (version.empty()) return;
        _major = 0;
        _minor = 0;
        LocalModuleFunction();
        this->memberFunction();
        Hypervisor::StaticMemberFunction();
    }


