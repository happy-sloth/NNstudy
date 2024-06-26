#include "matrix.hpp"
#include <x86intrin.h>
#include <cstdint>
#include <cstring>
#include <new>

Matrix::Matrix(unsigned int a_rows, unsigned int a_columns) : rows(a_rows), columns(a_columns)
{
    this->matrix = new double*[this->rows];

    // this->matrix = new(this->rows*(double*), std::align_val_t(32));

    for (unsigned int i = 0; i < this->rows; i++){
        // this->matrix[i] = new double[this->columns];
        this->matrix[i] = (double*)aligned_alloc(16, this->columns*sizeof(double));
    }
}

Matrix::Matrix(unsigned int a_rows, unsigned int a_columns, double value) : rows(a_rows), columns(a_columns)
{
    matrix = new double*[rows];

    for (unsigned int i = 0; i < rows; i++){
        // matrix[i] = new double[columns];
        this->matrix[i] = (double*)aligned_alloc(16, this->columns*sizeof(double));
    }

    for (unsigned int i = 0; i < rows; i++){
        for(unsigned int j = 0; j < columns; j++){
            matrix[i][j] = value;
        }
    }
}

Matrix::Matrix(const Matrix &mtx) : rows(mtx.rows), columns(mtx.columns)
{
    matrix = new double*[rows];

    for (unsigned int i = 0; i < rows; i++){
        // matrix[i] = new double[columns];
        this->matrix[i] = (double*)aligned_alloc(16, this->columns*sizeof(double));
    }

    for (unsigned int i = 0; i < rows; i++){
        std::memcpy( matrix[i], mtx.matrix[i], sizeof(double)*columns);
    }

    // for (unsigned int i = 0; i < rows; i++){
    //     for(unsigned int j = 0; j < columns; j++){
    //         matrix[i][j] = mtx.matrix[i][j];
    //     }
    // }
}

Matrix::~Matrix()
{
    for (unsigned int i = 0; i < rows; i++){
        // delete [] matrix[i];
        free(matrix[i]);
    }
    delete [] matrix;
}

Matrix Matrix::dot(Matrix &mtx1, Matrix &mtx2)
{
    if (mtx1.get_columns() != mtx2.get_rows())
        return mtx1;

    unsigned mtx1_rows = mtx1.get_rows();
    unsigned mtx1_col = mtx1.get_columns();
    unsigned int l_output_columns = mtx2.get_columns();
    Matrix l_matrix(mtx1_rows, l_output_columns);
    // Matrix mtx_2_t = mtx2.getTranspose();

    // for (unsigned int k = 0; k < l_output_columns; k++){
    //     for (unsigned int i = 0; i < mtx1_rows; i++){
    //         double l_temp = 0.0;
    //         for(unsigned int j = 0; j < mtx1_col; j++){
    //             l_temp += mtx1[i][j] * mtx2[j][k];
    //         }
    //         *(*(l_matrix.matrix + i) + k) = l_temp;
    //     }
    // }

    int mtx1_col_even_idx = mtx1_col - (mtx1_col - 1)%4;
    if(mtx1_col_even_idx >= 4){
        for (unsigned int k = 0; k < l_output_columns; k++){
            for (unsigned int i = 0; i < mtx1_rows; i++){
                double l_temp = 0.0;
                for(unsigned int j = 0; j < mtx1_col_even_idx-4; j+=4){
                    auto first_vector = _mm_load_pd(&mtx1[i][j]);
                    auto second_vector = _mm_set_pd(mtx2[j+1][k], mtx2[j][k]);

                    auto output_vector_1 = _mm_mul_pd(first_vector, second_vector);//_mm_dp_pd(first_vector,second_vector,0xF1);

                    first_vector = _mm_load_pd(&mtx1[i][j+2]);
                    second_vector = _mm_set_pd(mtx2[j+3][k], mtx2[j+2][k]);

                    auto output_vector_2 = _mm_mul_pd(first_vector, second_vector);//_mm_dp_pd(first_vector,second_vector,0xF1);

                    auto sum_vector = _mm_add_pd(output_vector_1, output_vector_2);
                    double a[2] = {};
                    _mm_store_pd(&a[0], sum_vector);
                    l_temp +=  a[0] + a[1];
                }
                if (mtx1_col_even_idx != mtx1_col){
                    for (int j = mtx1_col_even_idx; j < mtx1_col; j++)
                        l_temp += mtx1[i][j] * mtx2[j][k];
                }
                l_matrix.matrix[i][k] = l_temp;
            }
        }
    } else if(mtx1_col_even_idx == 1){
        unsigned out_mtx_cols_even_idx = l_output_columns - (l_output_columns - 1)%2;
        for (unsigned int k = 0; k < mtx1_rows; k++){
            auto second_vector = _mm_loaddup_pd(&mtx1[k][0]);
            for (unsigned int i = 0; i < out_mtx_cols_even_idx ; i+=2){
                auto first_vector = _mm_load_pd(&mtx2[0][i]);
                auto output_vector_2 = _mm_mul_pd(first_vector, second_vector);
                _mm_store_pd(&l_matrix.matrix[k][i], output_vector_2);
                // l_matrix.matrix[k][i] = mtx1[k][0]*mtx2[0][i];
            }
            if (out_mtx_cols_even_idx != l_output_columns)
                l_matrix.matrix[k][l_output_columns-1] = mtx1[k][0]*mtx2[0][out_mtx_cols_even_idx-1];
        }
    } else {
        for (unsigned int k = 0; k < l_output_columns; k++){
            for (unsigned int i = 0; i < mtx1_rows; i++){
                double l_temp = 0.0;
                for(unsigned int j = 0; j < mtx1_col; j++){
                    l_temp += mtx1[i][j] * mtx2[j][k];
                }
                *(*(l_matrix.matrix + i) + k) = l_temp;
            }
        }

    }
    return l_matrix;
}

Matrix Matrix::operator* (double scalar)
{
    Matrix temp(*this);

    int col_even_idx = temp.columns - (temp.columns - 1)%2;

    for (unsigned int i = 0; i < temp.rows; i++){
        for(unsigned int j = 0; j < col_even_idx; j++){
            auto first_vector = _mm_load_pd(*(temp.matrix +i)+j);
            auto second_vector = _mm_loaddup_pd(&scalar);

            auto sum_vector = _mm_mul_pd(first_vector, second_vector);

            _mm_store_pd(*(temp.matrix +i)+j, sum_vector);
            // temp.matrix[i][j] *= scalar;
        }
        if(col_even_idx != columns)
            *(*(temp.matrix + i)+(columns-1)) *= scalar;
    }

    return temp;
}

void Matrix::operator*= (double scalar)
{

    int col_even_idx = columns - (columns - 1)%2;

    for (unsigned int i = 0; i < rows; i++){
        for(unsigned int j = 0; j < col_even_idx; j+=2){
            auto first_vector = _mm_load_pd(*(matrix +i)+j);
            auto second_vector = _mm_loaddup_pd(&scalar);

            auto sum_vector = _mm_mul_pd(first_vector, second_vector);

            _mm_store_pd(*(matrix +i)+j, sum_vector);
            // matrix[i][j] *= scalar;
        }
        if(col_even_idx != columns)
            *(*(matrix + i)+(columns-1)) *= scalar;
            // matrix[i][columns] *= scalar;
    }
}

Matrix Matrix::operator* (Matrix &mtrx)
{
    Matrix out_matrix(mtrx.get_rows(), mtrx.get_columns());

    for (unsigned int i = 0; i < rows; i++){
        for(unsigned int j = 0; j < columns; j++){

            matrix[i][j] *= mtrx.matrix[i][j];
        }
    }

    return out_matrix;      
}

void Matrix::operator*= (Matrix &mtrx)
{
    int col_even_idx = columns - (columns - 1)%2;

    for (unsigned int i = 0; i < rows; i++){
        for(unsigned int j = 0; j < col_even_idx; j+=2){
            auto first_vector = _mm_load_pd(&matrix[i][j]);
            auto second_vector = _mm_load_pd(&mtrx[i][j]);

            auto sum_vector = _mm_mul_pd(first_vector, second_vector);

            _mm_store_pd(&matrix[i][j], sum_vector);
            // matrix[i][j] *= mtrx.matrix[i][j];
        }
        if(col_even_idx != columns)
            matrix[i][columns] *= mtrx.matrix[i][columns];
    }  
}

Matrix Matrix::operator+ (Matrix &mtrx)
{

}

void Matrix::operator+= (Matrix &m2)
{
    int col_even_idx = columns - (columns - 1)%2;

    for (unsigned int i = 0; i < rows; i++){
        for(unsigned int j = 0; j < col_even_idx; j+=2){
            auto first_vector = _mm_load_pd(*(matrix + i) + j);
            auto second_vector = _mm_load_pd(*(m2.matrix + i) + j);

            auto sum_vector = _mm_add_pd(first_vector, second_vector);

            _mm_store_pd(*(matrix + i) + j, sum_vector);

            // matrix[i][j] += m2.matrix[i][j];
        }
        if(col_even_idx != columns)
            *(*(matrix + i) + (columns-1)) += *(*(m2.matrix + i) + (columns-1));
    }
}

Matrix Matrix::operator- (Matrix&)
{

}

void Matrix::operator-= (Matrix &m2)
{
    for (unsigned int i = 0; i < rows; i++){
        for(unsigned int j = 0; j < columns; j++){
            matrix[i][j] -= m2.matrix[i][j];
        }
    }
}

Matrix Matrix::operator= (Matrix &m2)
{
    this->rows = m2.rows;
    this->columns = m2.columns;

    this->matrix = new double*[this->rows];

    for (unsigned int i = 0; i < this->rows; i++){
        // this->matrix[i] = new double[this->columns];
        this->matrix[i] = (double*)aligned_alloc(16, this->columns*sizeof(double));
    }

    // for (unsigned int i = 0; i < this->rows; i++){
    //     for(unsigned int j = 0; j < this->columns; j++){
    //         this->matrix[i][j] = m2.matrix[i][j];
    //     }
    // }

    for (unsigned int i = 0; i < rows; i++){
        std::memcpy( matrix[i], m2.matrix[i], sizeof(double)*columns);
    }

    return *this;
}

double* Matrix::operator[] (unsigned int idx)
{
    return matrix[idx];
}

double Matrix::getDeterminant() const
{

}

Matrix Matrix::getTranpose() const
{
    Matrix new_mtx(columns, rows);

    for (unsigned int i = 0; i < columns; i++){
        for(unsigned int j = 0; j < rows; j++){
            new_mtx[i][j] = matrix[j][i];
        }
    }

    return new_mtx;
}

void Matrix::transpose()
{
    double **t_matrix = matrix;

    matrix = new double*[columns];

    for (unsigned int i = 0; i < columns; i++){
        // matrix[i] = new double[rows];
        this->matrix[i] = (double*)aligned_alloc(16, this->rows*sizeof(double));
    }

    for (unsigned int i = 0; i < columns; i++){
        for(unsigned int j = 0; j < rows; j++){
            matrix[i][j] = t_matrix[j][i];
        }
    }

    for (unsigned int i = 0; i < rows; i++){
        // delete [] t_matrix[i];
        free(t_matrix[i]);
    }
    delete [] t_matrix;

    unsigned int temp = rows;
    rows = columns;
    columns = temp;
}

Matrix Matrix::getTranspose()
{
    Matrix outputMatrix(*this);
    outputMatrix.transpose();
    return outputMatrix;
}