#ifndef MACROS_H
# define MACROS_H

# define sizeof_in_bits(type) (sizeof(type) * 8)
# define sizeof_array(type) (sizeof(type) / sizeof(*(type)))
# define sizeof_str(str) (sizeof(str) - 1)

# define STRBUFFER(str) (str), sizeof_str((str))

# define NO_COPY(obj) obj(const obj&); obj& operator=(const obj&)

# define BIT(n) (1 << (n))

# define PRINTF(m, s) __attribute__((format (printf, m, s)))

# define STR_OR_NULL(str, meth) ((str) ? ((str)->meth) : "(null)")

#endif /* MACROS_H */
