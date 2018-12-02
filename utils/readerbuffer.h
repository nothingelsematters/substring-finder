#ifndef READERBUFFER_H
#define READERBUFFER_H

#include <QFile>

class ReaderBuffer {
public:
    ReaderBuffer();
    ~ReaderBuffer();

    void set_size(size_t buffer_size);
    char operator[](size_t i) const;
    size_t read_from_file(QFile* file, size_t index);

    size_t start_index = 0;
    size_t end_index = 0;
private:
    char* buffer = nullptr;
    size_t buffer_size;
};

#endif // READERBUFFER_H
