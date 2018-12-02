#include "readerbuffer.h"

ReaderBuffer::ReaderBuffer() {}

void ReaderBuffer::set_size(size_t buffer_size) {
    this->buffer_size = buffer_size;
    buffer = new char[buffer_size];
}

char ReaderBuffer::operator[](size_t i) const {
    return buffer[i - start_index];
}

size_t ReaderBuffer::read_from_file(QFile* file, size_t index) {
    start_index = index;
    end_index = index + file->read(buffer, buffer_size);
    return (start_index == end_index ? index : end_index + 1);
}

ReaderBuffer::~ReaderBuffer() {
    delete[] buffer;
}
