#include "filereader.h"

FileReader::FileReader(QString const& file_name, size_t buffer_size)
    : file(file_name) {
    this->buffer_size = std::max(static_cast<size_t>(100), buffer_size);
    first_buffer.set_size(this->buffer_size);
    second_buffer.set_size(this->buffer_size);
}

FileReader::~FileReader() {
    file.close();
}

bool FileReader::open() {
    return file.open(QIODevice::ReadOnly | QIODevice::Text);
}

bool FileReader::readable(size_t i) {
    if (i < index) {
        return true;
    }
    size_t new_index = (using_first ? second_buffer.read_from_file(&file, index) :
                           first_buffer.read_from_file(&file, index));
    if (new_index <= index) {
        return false;
    }
    index = new_index;
    using_first = !using_first;
    return readable(i);
}

char FileReader::operator[](size_t i) {
    if (i >= first_buffer.start_index && i < first_buffer.end_index) {
        return first_buffer[i];
    } else {
        return second_buffer[i];
    }
}
