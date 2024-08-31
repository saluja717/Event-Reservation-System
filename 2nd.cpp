#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_QUESTIONS 10
#define MAX_ANSWER_LENGTH 2
#define MAX_STUDENTS 10
#define MAX_OPTIONS 5

// Message structure to put in queue
struct Msg {
    long mtype;
    char mtext[MAX_ANSWER_LENGTH];
};

char generateRandomAnswer(int options) {
    if(options>MAX_OPTIONS){
        std::cerr<<"Invalid number of options"<<std::endl;
        return '0';
    }
    char answers[] = {'A', 'B', 'C', 'D', 'E'};
    return answers[rand() % options];
}

char assignGrade(int scored,int total) {
    double marks=(double)scored/(double)total;
    if (marks >= 0.70)
        return 'A';
    else if (marks >= 0.50)
        return 'B';
    else if (marks >= 0.30)
        return 'C';
    else
        return 'F';
}

int main() {
    int num_students, num_questions;
    std::cout << "Enter the number of students: ";
    std::cin >> num_students;
    std::cout << "Enter the number of questions: ";
    std::cin >> num_questions;

    if (num_students <= 0 || num_students > MAX_STUDENTS || num_questions <= 0 || num_questions > MAX_QUESTIONS) {
        std::cerr << "Invalid number of students or questions." << std::endl;
        return 1;
    }

    // Generate correct answers randomly
    srand(time(NULL));
    char correct_answers[MAX_QUESTIONS];
    int options_for_question[MAX_QUESTIONS];
    for (int i = 0; i < num_questions; ++i) {
        options_for_question[i] = 1+rand()%MAX_OPTIONS;
        correct_answers[i] = generateRandomAnswer(options_for_question[i]);
    }

    // Fork child processes for students
    for (int i = 0; i < num_students; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Failed to fork process." << std::endl;
            return 1;
        } else if (pid == 0) { // Child process
            // Create queue  
            key_t key = ftok("exam", i + 1);
            int msgid = msgget(key, 0666 | IPC_CREAT);
            if (msgid == -1) {
                std::cerr << "Failed to create message queue." << std::endl;
                return 1;
            }

            // Receives questions and sends answers
            Msg question_msg, answer_msg;
            for (int j = 0; j < num_questions; ++j) {
                msgrcv(msgid, &question_msg, sizeof(question_msg.mtext), 1, 0);
                char answer = generateRandomAnswer(question_msg.mtext[0]-'0');
                answer_msg.mtype = 2;
                answer_msg.mtext[0] = answer;
                msgsnd(msgid, &answer_msg, sizeof(answer_msg.mtext), 0);
            }
            exit(0);
        }
    }

    // Parent process
    int student_grades[MAX_STUDENTS] = {0};
    for (int i = 0; i < num_questions; ++i) {
        for (int j = 0; j < num_students; ++j) {
            // Get queue
            key_t key = ftok("exam", j + 1);
            int msgid = msgget(key, 0666);
            if (msgid == -1) {
                std::cerr << "Failed to get message queue." << std::endl;
                return 1;
            }

            // Send question
            Msg question_msg;
            question_msg.mtype = 1;
            question_msg.mtext[0]='0'+options_for_question[i];
            msgsnd(msgid, &question_msg, sizeof(question_msg.mtext), 0);

            // Receive answer
            Msg answer_msg;
            msgrcv(msgid, &answer_msg, sizeof(answer_msg.mtext), 2, 0);

            // Grade the answer
            if (correct_answers[i] == answer_msg.mtext[0]) {
                student_grades[j]++;
            }
        }
    }

    // Display student grades
    std::cout << "Grades:" << std::endl;
    for (int i = 0; i < num_students; ++i) {
        int marks = student_grades[i] * 10;
        std::cout << "Student " << i + 1 << ": " << marks << "/"<<(num_questions*10)<< " Grade: " << assignGrade(marks,num_questions*10) << std::endl;
    }

    // Wait for all child processes to complete
    for (int i = 0; i < num_students; ++i) {
        wait(NULL);
    }

    return 0;
}
