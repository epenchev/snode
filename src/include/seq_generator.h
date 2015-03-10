//
// seq_generator.h
// Copyright (C) 2013  Emil Penchev, Bulgaria

#ifndef SEQ_GENERATOR_H_
#define SEQ_GENERATOR_H_

class sequence_id_generator
{
public:
    static sequence_id_generator& instance()
    {
        static sequence_id_generator s_generator;
        return s_generator;
    }

    unsigned next()
    {
        return ++m_sequence_id;
    }

private:
    sequence_id_generator() : m_sequence_id(0) {}
    unsigned long m_sequence_id;
};

#endif /* SEQ_GENERATOR_H_ */
