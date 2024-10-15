#define _CRT_SECURE_NO_WARNINGS


#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <gl/glut.h>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>

#define AC_RED "\x1b[31m"
#define AC_GREEN "\x1b[32m"
#define AC_YELLOW "\x1b[33m"
#define AC_BLUE "\x1b[34m"
#define AC_CYAN "\x1b[36m"
#define AC_NORMAL "\x1b[m"
#define AC_PAGE_TABLE "\x1b[39;1;46m"

using namespace std;

#define NUMBER_OF_FRAMES 30

#define MEMORY_DEFAULT_CHAR '$'

int terminate_program = 0;

int execute_step = 0;
bool execute_called = false;
bool glut_initialized = false;
string message;
int program_number = 0;

typedef struct PCB
{
    int TTC;        // Total time counter
    int TTL;        // ltotal time limit
    int LLC;        // line limit counter
    int TLL;        // total line limit
    int Process_ID; // use to debug ig?
} PCB;

PCB pcb;
// memory
char memory[300][4];
int occupied_frames_indicator[NUMBER_OF_FRAMES] = { 0 };

// CPU items
char general_register[4];
char instruction_register[4];
char instruction_counter[2];
char counter[1];
char page_table_register[2]; // frame number of page table in memory

// Interrupts
int SI = 0;
int PI = 0;
int TI = 0;

// fileptr
std::ifstream inputFile("Hello_File.osf");
std::ofstream outputFile("output.txt", std::ios::app);

//------------------------------------------------------------------------------

std::string read_word(int real_address)
{
    std::string word;
    for (int i = 0; i < 4; i++)
    {
        word = word + memory[real_address][i];
    }
    return word;
}

void write_word(int real_address, std::string word)
{
    for (int i = 0; i < 4; i++)
    {
        memory[real_address][i] = word[i];
    }
}

void print_memory()
{
    for (int i = 0; i < 300; i++)
    {
        std::cout << "\n"
            << i << " ";
        for (int j = 0; j < 4; j++)
        {
            std::cout << memory[i][j] << " ";
        }
    }
}

//----------------------------------------------------------------------------

void initialization()
{
    srand(time(NULL));
    for (int i = 0; i < NUMBER_OF_FRAMES; i++)
    {
        occupied_frames_indicator[i] = 0;
    }
    for (int i = 0; i < 300; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            memory[i][j] = MEMORY_DEFAULT_CHAR;
        }
    }
    SI = 0;
    TI = 0;
    PI = 0;
    for (int i = 0; i < 4; i++)
    {
        general_register[i] = '0';
        instruction_register[i] = '0';
        if (i < 2)
        {
            instruction_counter[i] = 0;
            page_table_register[i] = '0';
        }
        if (i < 1)
        {
            counter[i] = 0;
        }
    }
}

// use this for page fault to assign page
int get_random_frame()
{
    int random_frame = 0;
get:
    random_frame = rand() % NUMBER_OF_FRAMES;
    if (occupied_frames_indicator[random_frame] == 1)
    {
        // occupied
        goto get;
    }
    else
    {
        // got an non overlapping random frame
        occupied_frames_indicator[random_frame] = 1;
        return random_frame;
    }
}

void increment_instruction_counter()
{
    int counter = instruction_counter[0] * 10 + instruction_counter[1];
    counter++;
    instruction_counter[0] = counter / 10;
    instruction_counter[1] = counter % 10;
}

void decrement_instruction_counter()
{
    int counter = instruction_counter[0] * 10 + instruction_counter[1];
    counter--;
    instruction_counter[0] = counter / 10;
    instruction_counter[1] = counter % 10;
}

int get_page_table_register_as_int()
{
    return (page_table_register[0] - '0') * 10 + page_table_register[1] - '0';
}

int get_instruction_register_operand_as_int()
{
    return (instruction_register[2] - '0') * 10 + instruction_register[3] - '0';
}

int address_map(int orig_address)
{
    // must take a 0-99 int real address and output 0-299 virtual address from the page table
    int orig_address_tensplace = orig_address / 10;
    int mem_loc_of_page_table = get_page_table_register_as_int();
    // cout<<mem_loc_of_page_table<<endl;
    if (memory[mem_loc_of_page_table * 10 + orig_address_tensplace][3] == '$')
    {
        PI = 3; // Page fault
        return -1;
    }
    else
    {
        // cout<<(memory[mem_loc_of_page_table*10 + orig_address_tensplace][2]-'0')*10<<endl;
        return ((memory[mem_loc_of_page_table * 10 + orig_address_tensplace][2] - '0') * 10 + memory[mem_loc_of_page_table * 10 + orig_address_tensplace][3] - '0') * 10 + orig_address % 10;
    }
}

void terminate(int error_message)
{
    outputFile << "\n\n";

    char buffer[1000];
    switch (error_message)
    {
    case 0:
        outputFile << "Program executed successfully\n";
        cout << "OUTPUTFILE : Program executed successfully\n";
        message = "Program executed successfully";
        break;
    case 1:
        outputFile << "Out of data\n";
        message = "Out of data";
        break;
    case 2:
        outputFile << "Line Limit Exceeded\n";
        message = "Line Limit Exceeded";
        break;
    case 3:
        outputFile << "Time limit Exceeded\n";
        message = "Time Limit Exceeded";
        break;
    case 4:
        outputFile << "Operation code error\n";
        message = "Operation code error";
        break;
    case 5:
        outputFile << "Operand error\n";
        message = "Operand error";
        break;
    case 6:
        outputFile << "Invalid page fault\n";
        message = "Invalid page fault";
        break;

    default:
        break;
    }

    sprintf(buffer, "General Register: %c%c%c%c | Instruction Register: %c%c%c%c | Instruction Counter: %d%d | Counter: %d | Page Table Register: %c%c",
        general_register[0], general_register[1], general_register[2], general_register[3],
        instruction_register[0], instruction_register[1], instruction_register[2], instruction_register[3],
        instruction_counter[0], instruction_counter[1],
        counter[0],
        page_table_register[0], page_table_register[1]);

    outputFile << buffer << "\n\n";
    // cout<<buffer;
    terminate_program = 1;
}

void terminate(int error_message1, int error_message2)
{

    outputFile << "\n\n";

    char buffer[1000];

    switch (error_message1)
    {
    case 0:
        outputFile << "Program executed successfully\n";
        cout << "OUTPUTFILE : Program executed successfully\n";
        message = "Program executed successfully";
        break;
    case 1:
        outputFile << "Out of data\n";
        message = "Out of data";
        break;
    case 2:
        outputFile << "Line Limit Exceeded\n";
        message = "Line Limit Exceeded";
        break;
    case 3:
        outputFile << "Time limit Exceeded\n";
        message = "Time Limit Exceeded";
        break;
    case 4:
        outputFile << "Operation code error\n";
        message = "Operation code error";
        break;
    case 5:
        outputFile << "Operand error\n";
        message = "Operand error";
        break;
    case 6:
        outputFile << "Invalid page fault\n";
        message = "Invalid page fault";
        break;

    default:
        break;
    }

    outputFile << "\t";

    outputFile << "\n\n";

    switch (error_message2)
    {
    case 0:
        outputFile << "Program executed successfully\n";
        cout << "OUTPUTFILE : Program executed successfully\n";
        message = "Program executed successfully";
        break;
    case 1:
        outputFile << "Out of data\n";
        message = "Out of data";
        break;
    case 2:
        outputFile << "Line Limit Exceeded\n";
        message = "Line Limit Exceeded";
        break;
    case 3:
        outputFile << "Time limit Exceeded\n";
        message = "Time Limit Exceeded";
        break;
    case 4:
        outputFile << "Operation code error\n";
        message = "Operation code error";
        break;
    case 5:
        outputFile << "Operand error\n";
        message = "Operand error";
        break;
    case 6:
        outputFile << "Invalid page fault\n";
        message = "Invalid page fault";
        break;

    default:
        break;
    }

    sprintf(buffer, "General Register: %c%c%c%c | Instruction Register: %c%c%c%c | Instruction Counter: %d%d | Counter: %d | Page Table Register: %c%c",
        general_register[0], general_register[1], general_register[2], general_register[3],
        instruction_register[0], instruction_register[1], instruction_register[2], instruction_register[3],
        instruction_counter[0], instruction_counter[1],
        counter[0],
        page_table_register[0], page_table_register[1]);

    outputFile << buffer << "\n\n";
    terminate_program = 1;
}

void read()
{
    // string to store each line from the input file
    std::string data_line_buffer;
    std::getline(inputFile, data_line_buffer);
    // cout<<data_line_buffer<<endl;
    if (data_line_buffer.substr(0, 4) == "$END")
    {
        // $END terminate the program
        terminate(1);
    }
    else
    {
        // calculating the real address(RA)
        int VA = (instruction_register[2] - '0') * 10 + instruction_register[3] - '0';
        cout << VA << endl;
        int RA = address_map(VA);
        RA = RA / 10 * 10;
        cout << data_line_buffer << endl;
        for (int i = 0; i < 40; i++)
        {
            if (i + 1 > data_line_buffer.size())
                break;
            if (data_line_buffer.at(i) == '\n' || data_line_buffer.at(i) == '\0')
                break;
            memory[RA + i / 4][i % 4] = data_line_buffer.at(i);
            // cout<<RA+i/4<<i%4;
        } // cout<<endl;
    }     // print_memory();
}

void write()
{
    pcb.LLC++;
    // if the number of lines in the program exceed the total line limit then terminate
    if (pcb.LLC > pcb.TLL)
    {
        terminate(2);
        return;
    }
    // output file in append mode where the output is to be written
    int VA = (instruction_register[2] - '0') * 10 + instruction_register[3] - '0';
    // cout<<VA<<endl;
    int RA = address_map(VA);
    // cout<<RA<<endl;
    // writing in the output file from memory block character by character
    // print_memory();
    for (int i = 0; i < 40; i++)
    {
        if (memory[RA + i / 4][i % 4] == '$')
            continue;
        outputFile << memory[RA + i / 4][i % 4];
        // cout<<memory[RA+i/4][i%4];
    }

    // newline
    outputFile << "\n";
}

void mos()
{
    message = "";
    if (TI == 0 && SI == 1)
    {
        // read
        read();
    }
    else if (TI == 0 && SI == 2)
    {
        // write
        write();
    }
    else if (TI == 0 && SI == 3)
    {
        // terminate-0
        terminate(0);
    }
    else if (TI == 2 && SI == 1)
    {
        // terminate-3
        terminate(3);
    }
    else if (TI == 2 && SI == 2)
    {
        // write, terminate-3
        write();
        terminate(3);
    }
    else if (TI == 2 && SI == 3)
    {
        // terminate-0
        terminate(0);
    }

    if (TI == 0 && PI == 1)
    {
        terminate(4);
    }
    else if (TI == 0 && PI == 2)
    {
        terminate(5);
    }
    else if (TI == 0 && PI == 3)
    {
        // If Page Fault Valid, ALLOCATE, update page Table, Adjust IC if necessary, EXECUTE USER PROGRAM OTHERWISE TERMINATE (6)
        int offset = get_instruction_register_operand_as_int(); // offset of where to allocate
        if ((instruction_register[0] == 'G' && instruction_register[1] == 'D') || (instruction_register[0] == 'S' && instruction_register[1] == 'R'))
        { // valid page fault
            message = "Page fault occurred at time: " + to_string(pcb.TTC);
            printf("Page fault occurred at time: %d\n", pcb.TTC);
            int frame_start = get_random_frame();
            memory[get_page_table_register_as_int() * 10 + offset / 10][0] = '0';
            memory[get_page_table_register_as_int() * 10 + offset / 10][1] = '0';
            memory[get_page_table_register_as_int() * 10 + offset / 10][2] = frame_start / 10 + '0';
            memory[get_page_table_register_as_int() * 10 + offset / 10][3] = frame_start % 10 + '0';
            decrement_instruction_counter();

            // print_memory();
            // execute user program
        }
        else
        {
            terminate(6);
            //cout << "12345dwdw";
        }
    }
    else if (TI == 2 && PI == 1)
    {
        terminate(3, 4);
        //cout << "1234599999";
    }
    else if (TI == 2 && PI == 2)
    {
        terminate(3, 5);
        //cout << "12345ddw";
    }
    else if (TI == 2 && PI == 3)
    {
        terminate(3);
    }

    SI = 0;
    PI = 0;
    TI = 0;
}

int check_if_valid_operand()
{
    
    if ((instruction_register[2] < '0' || instruction_register[3] < '0' || instruction_register[2] > '9' || instruction_register[3] > '9'))
    {
        if ((instruction_register[0] == 'G' && instruction_register[1] == 'D') || (instruction_register[0] == 'P' && instruction_register[1] == 'D') || (instruction_register[0] == 'L' && instruction_register[1] == 'R') || (instruction_register[0] == 'S' && instruction_register[1] == 'R') || (instruction_register[0] == 'C' && instruction_register[1] == 'R') || (instruction_register[0] == 'B' && instruction_register[1] == 'T')) {
            return 0;
        }
    }
    return 1;
}

int a = 0;


void load()
{
    bool valid_EOF = true;

    int frame;
    int lineITERATOR;
    int program_line_no = 0;
    int PA;
    int VA;
    int start = 0;

    if (!inputFile.is_open())
    {
        std::cerr << "Error: File not found or could not be opened." << std::endl;
        return;
    }

    std::string line;

    while (std::getline(inputFile, line))
    {
        if (line[0] != '\n') {
            valid_EOF = false;
        }
        lineITERATOR = 0;
        // std::cout<<line<<std::endl;
        if (line.substr(lineITERATOR, 4) == "$DTA")
        {
            terminate_program = 0;
            //            print_memory();
            execute();
            execute_called = true;
            start = 0;
            break;
            // need to start execution here
        }
        else if (line.substr(lineITERATOR, 4) == "$AMJ")
        {
            // printf("here");
            program_number++;
            start = 1;
            program_line_no = 0;
            initialization();
            lineITERATOR += 4;
            pcb.Process_ID = (line[lineITERATOR] - '0') * 1000 + (line[lineITERATOR + 1] - '0') * 100 + (line[lineITERATOR + 2] - '0') * 10 + (line[lineITERATOR + 3] - '0');
            pcb.LLC = 0;
            pcb.TTC = 0;
            lineITERATOR += 4;
            pcb.TTL = (line[lineITERATOR] - '0') * 1000 + (line[lineITERATOR + 1] - '0') * 100 + (line[lineITERATOR + 2] - '0') * 10 + (line[lineITERATOR + 3] - '0');
            //cout<<(line[lineITERATOR + 3] - '0')<<endl;
            lineITERATOR += 4;
            pcb.TLL = (line[lineITERATOR] - '0') * 1000 + (line[lineITERATOR + 1] - '0') * 100 + (line[lineITERATOR + 2] - '0') * 10 + (line[lineITERATOR + 3] - '0');
            lineITERATOR += 4;
            frame = get_random_frame();
            occupied_frames_indicator[frame] = 1;
            page_table_register[0] = frame / 10 + '0';
            page_table_register[1] = frame % 10 + '0';
        }
        else if (line.substr(lineITERATOR, 4) == "$END")
        {
            std::cout << "job done";
            initialization();
        }
        else if (start == 1)
        {
            // printf("here");
            frame = get_random_frame();
            occupied_frames_indicator[frame] = 1;
            // std::cout << "random frame: "<< frame * 10;
            PA = frame * 10;
            std::string word_to_write = "0000";
            word_to_write[2] = frame / 10 + '0';
            word_to_write[3] = frame % 10 + '0';
            // std::cout << get_page_table_register_as_int() * 10;
            write_word(get_page_table_register_as_int() * 10 + program_line_no, word_to_write);
            // std::cout << ""<< get_page_table_register_as_int()*10 + program_line_no;

            for (int i = 0; i < 40; i++)
            {
                if (i + 1 > line.size())
                    break;
                else if (line.at(i) == '\n' || line.at(i) == '\0')
                    break;
                else
                {
                    memory[frame * 10 + i / 4][i % 4] = line.at(i);
                }
            }

            program_line_no++;
        }
    }
    if (valid_EOF == true) {
        //print_memory_design();
        outputFile.close();
        inputFile.close();
        cout << "\nCLOSED\n";
        message = "Output written to file";
    }

}

void print_memory_design()
{
    // printf("__________________________");
    for (int i = 0; i < 300; i++)
    {
        printf("\n%s|-------------------------|%s\n%s|%s %3d %s|%s", AC_RED, AC_NORMAL, AC_RED, AC_GREEN, i, AC_RED, AC_NORMAL);
        if (i / 10 == get_page_table_register_as_int())
        {
            for (int j = 0; j < 4; j++)
            {
                if (j == 4 - 1)
                {
                    printf("%s  %c %s%s|%s", AC_PAGE_TABLE, memory[i][j], AC_NORMAL, AC_RED, AC_NORMAL);
                    continue;
                }
                printf("%s  %c %s%s|%s", AC_PAGE_TABLE, memory[i][j], AC_NORMAL, AC_YELLOW, AC_NORMAL);
            }
        }
        else
        {
            for (int j = 0; j < 4; j++)
            {
                if (j == 4 - 1)
                {
                    printf("%s  %c %s|%s", AC_BLUE, memory[i][j], AC_RED, AC_NORMAL);
                    continue;
                }
                printf("%s  %c %s|%s", AC_BLUE, memory[i][j], AC_YELLOW, AC_NORMAL);
            }
        }
    }
    printf("\n%s|-------------------------|%s\n", AC_RED, AC_NORMAL);
}











void mouseClick(int button, int state, int x, int y) {
    if (state == GLUT_UP) {
        return;
    }
    execute_step = 1;
    cout << "ExecuteCalled : " << execute_called << endl;
    while (execute_step == 1 && execute_called == true && terminate_program == 0)
    {
        cout<<"Time: "<<pcb.TTC<<endl;
        // cout<<"Total Time: "<<pcb.TTL<<endl;
        // printf("here");
        //  get address in real memory
        int VA = instruction_counter[0] * 10 + instruction_counter[1]; // number assumed
        int RA = address_map(VA);

        std::cout <<"VA : " << VA << std::endl;
        std::cout<<RA<<std::endl;
        // fill the IR with memory ops
        for (int i = 0; i < 4; i++)
        {
            instruction_register[i] = memory[RA][i];
        }
        cout << "Register : ";
        for (int i = 0; i < 4; i++)
        {
            std::cout << instruction_register[i];
        }
        std::cout << std::endl;

        // increment the counter for next
        VA++;
        instruction_counter[0] = VA / 10;
        instruction_counter[1] = VA % 10;

        // check for valid operand
        if (check_if_valid_operand() == 0)
        {
            // invalid operand
            // cout<<instruction_register<<endl;
            PI = 2;
        }
        else if (instruction_register[0] == 'G' && instruction_register[1] == 'D')
        {
            int virtual_operand = ((instruction_register[2] - '0') * 10 + instruction_register[3] - '0');
            // cout<<virtual_operand<<endl;
            if (address_map(virtual_operand) == -1)
            {
                PI = 3;
                // cout<<"aaa"<<endl;
                // cout<<"aaa"<<endl;
            }
            else
            {
                SI = 1;
            }
        }
        else if (instruction_register[0] == 'P' && instruction_register[1] == 'D')
        {
            int virtual_operand = ((instruction_register[2] - '0') * 10 + instruction_register[3] - '0');
            if (address_map(virtual_operand) == -1)
            {
                PI = 3;
            }
            else
            {
                SI = 2;
            }
        }
        else if (instruction_register[0] == 'L' && instruction_register[1] == 'R')
        {
            int virtual_address = (instruction_register[2] - '0') * 10 + (instruction_register[3] - '0');
            // cout<<"VA"<<virtual_address<<endl;
            int real_address = address_map(virtual_address);
            // cout<<"RA"<<real_address<<endl;
            if (real_address == -1)
            {
                PI = 3;
            }
            else
            {
                for (int i = 0; i < 4; i++)
                {
                    general_register[i] = memory[real_address][i];
                }
            } 
            // cout<<general_register<<endl;
            // check if page fault, if yes then perform appropriate task
            // here page dault = invalid page fault, terminate
            // else load from memory
        }
        else if (instruction_register[0] == 'S' && instruction_register[1] == 'R')
        {
            int virtual_address = (instruction_register[2] - '0') * 10 + (instruction_register[3] - '0');
            // cout<<"VA"<<virtual_address<<endl;
            int real_address = address_map(virtual_address);
            // cout<<"RA"<<real_address<<endl;
            if (real_address == -1)
            {
                PI = 3;
            }
            else
            {
                for (int i = 0; i < 4; i++)
                {
                    memory[real_address][i] = general_register[i];
                }
            }
            // check if page fault, if yes then perform appropriate task
            // then store in memory
        }
        else if (instruction_register[0] == 'B' && instruction_register[1] == 'T')
        {
            if (counter[0] == 1)
            {
                instruction_counter[0] = instruction_register[2] - '0';
                instruction_counter[1] = instruction_register[3] - '0';
            }
        }
        else if (instruction_register[0] == 'C' && instruction_register[1] == 'R')
        {
            int virtual_address = (instruction_register[2] - '0') * 10 + (instruction_register[3] - '0');
            int real_address = address_map(virtual_address);
            if (real_address == -1)
            {
                PI = 3;
            }
            else
            {
                counter[0] = 1;
                for (int i = 0; i < 4; i++)
                {
                    if (memory[real_address][i] != general_register[i])
                    {
                        counter[0] = 0;
                    }
                } // cout<<"Compare: "<< memory[real_address]<<" "<<general_register<<endl;
            }
            cout << "Counter: " << (char)(counter[0] + '0') << endl
                << endl;
            // check if page fault, if yes then perform appropriate task
            // here page dault = invalid page fault, terminate
            // else load from memory and do comparison
        }
        else if (instruction_register[0] == 'H')
        {
            SI = 3;
        }
        else
        {
            // invalid opcode
            PI = 1;
            cout << "InValidOpcode" << endl;
        }

        pcb.TTC += 1;
        if (pcb.TTL < pcb.TTC)
        {
            TI = 2;
        }

        if (SI != 0 || PI != 0 || TI != 0)
        {
            mos();
        }
        // a+=1;
        // if(a==16) {
        //     break;
        // }
        execute_step = 0;
    }
    if (terminate_program != 0) {
        execute_called = false;
        cout << "ExecuteEnd" << endl;
        load();
    }
}




void display() {



    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Page Table
    glColor4f(0.0, 1.0, 1.0, 0.5);
    glBegin(GL_POLYGON);
    int x = -740 + 150 * int((get_page_table_register_as_int()) / 3);
    int y = 390 - 250 * (get_page_table_register_as_int()%3);
    glVertex2f(x, y);
    glVertex2f(x, y-250);
    glVertex2f(x+130, y-250);
    glVertex2f(x+130, y);
    glEnd();

    // Program number
    glColor4f(1.0, 0.5, 0.2, 0.5);
    glBegin(GL_POLYGON);
    glVertex2f(600, -370);
    glVertex2f(738, -370);
    glVertex2f(738, -398);
    glVertex2f(600, -398);
    glEnd();


    // Memory
    glPointSize(1.0);
    glColor3f(0.0, 0.0, 0.0);
    for (int j = 0; j < 10; j++) {
        int left = -740 + j * 150;
        glBegin(GL_LINE_LOOP);
        glVertex2f(left, 390);
        glVertex2f(left, -360);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glVertex2f(left+130, 390);
        glVertex2f(left+130, -360);
        glEnd();
        for (int i = 0; i < 4; i++) {
            glBegin(GL_LINE_LOOP);
            glVertex2f(left+30 + i * 25, 390);
            glVertex2f(left+30 + i * 25, -360);
            glEnd();
        }
        for (int i = 0; i < 31; i++) {
            glBegin(GL_LINE_LOOP);
            glVertex2f(left, -360 + i * 25);
            glVertex2f(left+130, -360 + i * 25);
            glEnd();
        }
    }

    for (int k = 0; k < 10; k++) {
        int left = -738 + k * 150;
        for (int i = k*30; i < k*30+30; i++) {
            glRasterPos2i(left, 370 - (i-k*30) * 25);
            string s = to_string(i);
            void* font = GLUT_BITMAP_9_BY_15;
            for (string::iterator j = s.begin(); j != s.end(); ++j)
            {
                char c = *j;
                glutBitmapCharacter(font, c);
            }
            for (int l = 0; l < 4; l++) {
                glRasterPos2i(left + 12+25*(l+1), 370 - (i - k * 30) * 25);
                if (memory[i][l] == '$') {
                    glutBitmapCharacter(font, ' ');
                }
                else {
                    glutBitmapCharacter(font, memory[i][l]);
                }
            }
        }
    }


    glRasterPos2i(-725, -390);
    string genRegister = "General Register: ";
    void* font = GLUT_BITMAP_9_BY_15;
    for (string::iterator j = genRegister.begin(); j != genRegister.end(); ++j)
    {
        char c = *j;
        glutBitmapCharacter(font, c);
    }
    for (int i = 0; i < 4; i++) {
        glRasterPos2i(-575 + i * 10, -390);
        glutBitmapCharacter(font, general_register[i]);
    }


    glRasterPos2i(-495, -390);
    string instRegister = "Instruction Register: ";
    //void* font = GLUT_BITMAP_9_BY_15;
    for (string::iterator j = instRegister.begin(); j != instRegister.end(); ++j)
    {
        char c = *j;
        glutBitmapCharacter(font, c);
    }
    for (int i = 0; i < 4; i++) {
        glRasterPos2i(-305 + i * 10, -390);
        glutBitmapCharacter(font, instruction_register[i]);
    }


    glRasterPos2i(-195, -390);
    string instCounter = "Instruction Counter: ";
    //void* font = GLUT_BITMAP_9_BY_15;
    for (string::iterator j = instCounter.begin(); j != instCounter.end(); ++j)
    {
        char c = *j;
        glutBitmapCharacter(font, c);
    }
    for (int i = 0; i < 2; i++) {
        glRasterPos2i(-15 + i * 10, -390);
        glutBitmapCharacter(font, instruction_counter[i]+'0');
    }

    glRasterPos2i(75, -390);
    string counter_str = "Counter: ";
    //void* font = GLUT_BITMAP_9_BY_15;
    for (string::iterator j = counter_str.begin(); j != counter_str.end(); ++j)
    {
        char c = *j;
        glutBitmapCharacter(font, c);
    }
    glRasterPos2i(145, -390);
    glutBitmapCharacter(font, counter[0] + '0');

    glRasterPos2i(275, -390);
    //void* font = GLUT_BITMAP_9_BY_15;
    for (string::iterator j = message.begin(); j != message.end(); ++j)
    {
        char c = *j;
        glutBitmapCharacter(font, c);
    }


    
    glRasterPos2i(625, -390);
    string program_no_str = "Program: " + to_string(program_number);
    //void* font = GLUT_BITMAP_9_BY_15;
    for (string::iterator j = program_no_str.begin(); j != program_no_str.end(); ++j)
    {
        char c = *j;
        glutBitmapCharacter(font, c);
    }



    glFlush();

    if (glut_initialized == false) {
        glut_initialized = true;
        load();
    }
}


int main(int argc, char* argv[]) {

    glutInit(&argc, argv);
    glutInitWindowSize(1500, 800);
    glutInitWindowPosition(10, 10);
    glutCreateWindow("2D Transformations");
    glutFullScreen();
    glutDisplayFunc(display);
    glutIdleFunc(display);
    glutMouseFunc(mouseClick);
    glOrtho(-750, 750, -400, 400, -1, 1);
    //glutMouseFunc(getMouseCLicks);
    glutMainLoop();
    //start_time = time(NULL);


    return 0;
}
