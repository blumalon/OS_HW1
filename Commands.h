// Ver: 04-11-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <memory>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
    pid_t currentPID;
    std::string cmdLine;

public:
    explicit Command(const char *cmd_line , pid_t pid = -1) :
    cmdLine(cmd_line), currentPID(pid) {};

    std::string getCmdLine() const { return cmdLine; }
    pid_t getPid() const { return currentPID; }

    virtual ~Command();

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();

};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};


class RedirectionCommand : public Command {

public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    Command* firstCommand;
    Command* secondCommand;
    bool am_i_with_AND;
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class USBInfoCommand : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    USBInfoCommand(const char *cmd_line);

    virtual ~USBInfoCommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    char* moveTo;
public:
    ChangeDirCommand(char *path);

    virtual ~ChangeDirCommand() = default;

    void execute() override;
};

class ChangePrompt : public BuiltInCommand {
    std::string prompt;
public:
    explicit ChangePrompt(std::string const& prompt);

    virtual ~ChangePrompt() = default;


    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {

    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};


class JobsList {
    int getNextJobID();
public:
    class JobEntry {
        int jobId = 0;
        pid_t pid = -2;
        const std::string commandLine;
    public:
        int getJobId() const { return jobId; }
        pid_t getPid() const { return pid; }
        std::string getCommandLine() const { return commandLine; }
    };
std::vector<JobEntry> jobsVector;

public:
    JobsList();

    ~JobsList(){jobsVector.clear();};

    void addJob(Command *cmd, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    bool is_there_a_job_with_pid(const int pid);

    int getJobList_size() {
        removeFinishedJobs();
        return jobsVector.size();
    }
};

class JobsCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line) {}

    virtual ~JobsCommand();

    void execute() override {
        jobs->printJobsList();
    }
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
    const char* cmd_line;
public:
    explicit AliasCommand(const char *cmd_line);
    virtual ~AliasCommand() {
    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
    const char* cmd_line;
public:

    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line);

    virtual ~SysInfoCommand() {
    }

    void execute() override;
};

class SmallShell {
private:
    std::string currentPrompt = "smash";
    char* previousDir;
    std::vector<std::pair<std::string, std::string>> aliasVector;
    SmallShell();

public:
    char** getPreviousDirPtr() {return &previousDir;}

    void setPreviousDirPtr(char* ptr) {previousDir = ptr;}

    std::vector<std::pair<std::string, std::string>>& getAliasVector()
    {return aliasVector;}

    void printAlias();

    void addAlias(char** argv);

    void addAlias(char** argv, const char* cmd_line);
    Command *CreateCommand(const char *cmd_line);

    std::string getPrompt() const {
        return currentPrompt;
    }

    void setPrompt(std::string const & prompt) {
        currentPrompt = prompt;
    }

    SmallShell(SmallShell const &) = delete; // disable copy ctor

    void operator=(SmallShell const &) = delete; // disable = operator

    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
