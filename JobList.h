#pragma once
#include <memory>

class Job {
    int jobId;
    pid_t pid;
    const std::string commandLine;
    std::shared_ptr<Job> next = nullptr;
public:
    Job(int jobId, pid_t pid, const std::string commandLine)
        : jobId(jobId), pid(pid), commandLine(commandLine) {}
    int getID() const { return jobId; }
    pid_t getPID() const { return pid; }
    std::string getCommandLine() const { return commandLine; }
    std::shared_ptr<Job> getNext() const { return next; }
    void setNext(std::shared_ptr<Job> nextJob) { next = nextJob; }
};

class JobList {
    std::shared_ptr<Job> head = nullptr;
    std::shared_ptr<Job> tail = nullptr;
    int num_of_running_jobs = 0;
public:
    ~JobList() {
        std::shared_ptr<Job> curr = tail;
        if (curr == nullptr)
            return;
        std::shared_ptr<Job> next = tail->getNext();
        while (next != nullptr) {
            curr.reset();
            curr = next;
            next = next->getNext();
        }
    }

    bool is_there_a_job_with_pid(const int pid) {
        clean_dead_jobs();
        std::shared_ptr<Job> curr = tail;
        while (curr != nullptr) {
            if (curr->getPID() == pid) {
                return true;
            }
            curr = curr->getNext();
        }
        return false;
    }

    void insertJob(const std::shared_ptr<Job> &job) {
        if (tail == nullptr)
            tail = job;
        if (head == nullptr) {
            head = job;
        }else {
            head->setNext(job);
            head = job;
        }
        num_of_running_jobs++;
    }

    std::string deleteJob_byPID(const int pidToDelete = 0) {
        std::shared_ptr<Job> curr = tail;
        if(curr == nullptr && pidToDelete == 0)
            throw std::invalid_argument("smash error: fg: jobs list is empty");
        std::shared_ptr<Job> next = tail->getNext();
        std::string toReturn;
        while (next != nullptr) {
            if(next->getPID() == pidToDelete) {
                curr->setNext(next->getNext());
                if (head == next) {
                    if (next->getNext() == nullptr) {
                        head = curr;
                    }else {
                        head = next->getNext();
                    }
                }
                toReturn = next->getCommandLine();
                next.reset();
                num_of_running_jobs--;
                return toReturn;
            }
            curr = next;
            next = next->getNext();
        }
        toReturn = "job-id " + std::to_string(pidToDelete) + " does not exist";
        throw std::invalid_argument(toReturn);
    }
    int getNextJobID() {
        clean_dead_jobs();
        int max = 0;
        std::shared_ptr<Job> curr = tail;
        while (curr != nullptr) {
            if (curr->getID() > max) {
                max = curr->getID();
            }
            curr = curr->getNext();
        }
        return max + 1;
    }

    void clean_dead_jobs() {
        std::shared_ptr<Job> curr = tail;
        if (curr == nullptr)
            return;
        std::shared_ptr<Job> next_job = curr->getNext();
        while(next_job != nullptr) {
            int status;
            pid_t result = waitpid(next_job->getPID(), &status, WNOHANG);
            if (result != -1) {
                if (result > 0) {
                    curr->setNext(next_job->getNext());
                    if (head == next_job) {
                        if (next_job->getNext() == nullptr) {
                            head = curr;
                        }else {
                            head = next_job->getNext();
                        }
                    }
                    next_job.reset();
                    num_of_running_jobs--;
                    next_job = curr->getNext();
                } else {
                    curr = next_job;
                    next_job = next_job->getNext();
                }
            }
        }
    }

    std::string printJobs() {
        clean_dead_jobs();
        std::string result;
        std::shared_ptr<Job> curr = tail;
        while (curr != nullptr) {
            result += "[" + std::to_string(curr->getID()) + "] " +
                      curr->getCommandLine()  + "\n";
            curr = curr->getNext();
        }
        return result;
    }
};