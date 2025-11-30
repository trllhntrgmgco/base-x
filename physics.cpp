#include "physics.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BaseXEngine {
    
    // Initialize static member
    int BaseXConfig::currentBase = 16;
    
    // BaseXConfig implementation
    void BaseXConfig::setBase(int newBase) {
        if (newBase >= MIN_BASE && newBase <= MAX_BASE) {
            currentBase = newBase;
            std::cout << "Base-X Engine: Base set to " << currentBase << std::endl;
        }
    }
    
    int BaseXConfig::getBase() {
        return currentBase;
    }
    
    int BaseXConfig::getDimensions() {
        return std::min(currentBase, 16); // Cap practical dimensions
    }
    
    double BaseXConfig::getQuantumScale() {
        return static_cast<double>(currentBase) / 16.0;
    }
    
    // BaseXVector implementation
    BaseXVector::BaseXVector() : dimensions(BaseXConfig::getDimensions()) {
        components.resize(dimensions, 0.0);
    }
    
    BaseXVector::BaseXVector(double x, double y, double z) : dimensions(BaseXConfig::getDimensions()) {
        components.resize(dimensions, 0.0);
        if (dimensions >= 1) components[0] = x;
        if (dimensions >= 2) components[1] = y;
        if (dimensions >= 3) components[2] = z;
        
        // Fill higher dimensions with quantum-derived values
        for (int i = 3; i < dimensions; i++) {
            components[i] = (x + y + z) * std::sin(i * BaseXConfig::PHI) * 0.1;
        }
    }
    
    BaseXVector::BaseXVector(const std::vector<double>& values) {
        dimensions = BaseXConfig::getDimensions();
        components.resize(dimensions, 0.0);
        
        size_t copyCount = std::min(values.size(), static_cast<size_t>(dimensions));
        for (size_t i = 0; i < copyCount; i++) {
            components[i] = values[i];
        }
    }
    
    double BaseXVector::x() const {
        return dimensions >= 1 ? components[0] : 0.0;
    }
    
    double BaseXVector::y() const {
        return dimensions >= 2 ? components[1] : 0.0;
    }
    
    double BaseXVector::z() const {
        return dimensions >= 3 ? components[2] : 0.0;
    }
    
    void BaseXVector::setX(double value) {
        if (dimensions >= 1) components[0] = value;
    }
    
    void BaseXVector::setY(double value) {
        if (dimensions >= 2) components[1] = value;
    }
    
    void BaseXVector::setZ(double value) {
        if (dimensions >= 3) components[2] = value;
    }
    
    double BaseXVector::operator[](int index) const {
        return (index >= 0 && index < dimensions) ? components[index] : 0.0;
    }
    
    double& BaseXVector::operator[](int index) {
        if (index >= 0 && index < dimensions) {
            return components[index];
        }
        static double dummy = 0.0;
        return dummy;
    }
    
    int BaseXVector::getDimensions() const {
        return dimensions;
    }
    
    BaseXVector BaseXVector::operator+(const BaseXVector& other) const {
        BaseXVector result;
        for (int i = 0; i < dimensions; i++) {
            result.components[i] = components[i] + other[i];
        }
        return result;
    }
    
    BaseXVector BaseXVector::operator-(const BaseXVector& other) const {
        BaseXVector result;
        for (int i = 0; i < dimensions; i++) {
            result.components[i] = components[i] - other[i];
        }
        return result;
    }
    
    BaseXVector BaseXVector::operator*(double scalar) const {
        BaseXVector result;
        for (int i = 0; i < dimensions; i++) {
            result.components[i] = components[i] * scalar;
        }
        return result;
    }
    
    BaseXVector BaseXVector::operator/(double scalar) const {
        if (std::abs(scalar) < 1e-10) return *this; // Avoid division by zero
        return *this * (1.0 / scalar);
    }
    
    BaseXVector& BaseXVector::operator+=(const BaseXVector& other) {
        for (int i = 0; i < dimensions; i++) {
            components[i] += other[i];
        }
        return *this;
    }
    
    BaseXVector& BaseXVector::operator-=(const BaseXVector& other) {
        for (int i = 0; i < dimensions; i++) {
            components[i] -= other[i];
        }
        return *this;
    }
    
    BaseXVector& BaseXVector::operator*=(double scalar) {
        for (int i = 0; i < dimensions; i++) {
            components[i] *= scalar;
        }
        return *this;
    }
    
    BaseXVector& BaseXVector::operator/=(double scalar) {
        if (std::abs(scalar) > 1e-10) {  // Avoid division by zero
            for (int i = 0; i < dimensions; i++) {
                components[i] /= scalar;
            }
        }
        return *this;
    }
    
    double BaseXVector::dot(const BaseXVector& other) const {
        double result = 0.0;
        int minDim = std::min(dimensions, other.dimensions);
        for (int i = 0; i < minDim; i++) {
            result += components[i] * other.components[i];
        }
        return result;
    }
    
    BaseXVector BaseXVector::cross(const BaseXVector& other) const {
        BaseXVector result;
        if (dimensions >= 3) {
            result.setX(y() * other.z() - z() * other.y());
            result.setY(z() * other.x() - x() * other.z());
            result.setZ(x() * other.y() - y() * other.x());
            
            // Quantum cross-product effects in higher dimensions
            for (int i = 3; i < dimensions; i++) {
                double quantumCross = (components[i] * other.components[(i + 1) % dimensions] - 
                                     components[(i + 1) % dimensions] * other.components[i]) * 
                                     BaseXConfig::getQuantumScale();
                result.components[i] = quantumCross;
            }
        }
        return result;
    }
    
    double BaseXVector::magnitude() const {
        double sum = 0.0;
        for (int i = 0; i < dimensions; i++) {
            sum += components[i] * components[i];
        }
        return std::sqrt(sum);
    }
    
    double BaseXVector::magnitudeSquared() const {
        double sum = 0.0;
        for (int i = 0; i < dimensions; i++) {
            sum += components[i] * components[i];
        }
        return sum;
    }
    
    BaseXVector BaseXVector::normalized() const {
        double mag = magnitude();
        if (mag < 1e-10) return BaseXVector(); // Return zero vector if magnitude is too small
        return *this / mag;
    }
    
    void BaseXVector::normalize() {
        double mag = magnitude();
        if (mag > 1e-10) {
            *this /= mag;
        }
    }
    
    double BaseXVector::distanceTo(const BaseXVector& other) const {
        return (*this - other).magnitude();
    }
    
    double BaseXVector::distanceSquaredTo(const BaseXVector& other) const {
        return (*this - other).magnitudeSquared();
    }
    
    double BaseXVector::getQuantumPhase() const {
        double phase = 0.0;
        for (int i = 0; i < dimensions; i++) {
            phase += components[i] * std::sin(i * BaseXConfig::PHI);
        }
        return phase * BaseXConfig::getQuantumScale();
    }
    
    bool BaseXVector::isQuantumEntangled(const BaseXVector& other) const {
        double correlation = std::abs(dot(other)) / (magnitude() * other.magnitude());
        return correlation > (0.8 + 0.1 * BaseXConfig::getQuantumScale());
    }
    
    BaseXVector BaseXVector::getBaseXProjection(int targetBase) const {
        if (targetBase == BaseXConfig::currentBase) return *this;
        
        BaseXVector result;
        double scaleFactor = static_cast<double>(targetBase) / BaseXConfig::currentBase;
        
        for (int i = 0; i < std::min(dimensions, targetBase); i++) {
            result.components[i] = components[i] * scaleFactor;
        }
        
        return result;
    }
    
    void BaseXVector::print() const {
        std::cout << "BaseXVector (Base-" << BaseXConfig::currentBase << ", " << dimensions << "D): [";
        for (int i = 0; i < dimensions; i++) {
            std::cout << components[i];
            if (i < dimensions - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }
    
    // BaseXMatrix implementation
    BaseXMatrix::BaseXMatrix() : size(BaseXConfig::getDimensions()) {
        matrix.resize(size, std::vector<double>(size, 0.0));
        // Initialize as identity matrix
        for (int i = 0; i < size; i++) {
            matrix[i][i] = 1.0;
        }
    }
    
    BaseXMatrix::BaseXMatrix(int dimensions) : size(dimensions) {
        matrix.resize(size, std::vector<double>(size, 0.0));
        for (int i = 0; i < size; i++) {
            matrix[i][i] = 1.0;
        }
    }
    
    std::vector<double>& BaseXMatrix::operator[](int row) {
        return matrix[row];
    }
    
    const std::vector<double>& BaseXMatrix::operator[](int row) const {
        return matrix[row];
    }
    
    BaseXMatrix BaseXMatrix::operator*(const BaseXMatrix& other) const {
        BaseXMatrix result(size);
        
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                result.matrix[i][j] = 0.0;
                for (int k = 0; k < size; k++) {
                    result.matrix[i][j] += matrix[i][k] * other.matrix[k][j];
                }
            }
        }
        
        return result;
    }
    
    BaseXVector BaseXMatrix::transform(const BaseXVector& vec) const {
        BaseXVector result;
        
        for (int i = 0; i < size; i++) {
            double sum = 0.0;
            for (int j = 0; j < vec.getDimensions(); j++) {
                sum += matrix[i][j] * vec[j];
            }
            result[i] = sum;
        }
        
        return result;
    }
    
    BaseXMatrix BaseXMatrix::createRotationX(double angle) {
        BaseXMatrix result;
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);
        
        if (result.size >= 3) {
            result[1][1] = cosA;  result[1][2] = -sinA;
            result[2][1] = sinA;  result[2][2] = cosA;
        }
        
        // Apply quantum rotation effects to higher dimensions
        for (int i = 3; i < result.size; i++) {
            double quantumAngle = angle * BaseXConfig::getQuantumScale() * (i - 2);
            result[i][i] = std::cos(quantumAngle);
        }
        
        return result;
    }
    
    BaseXMatrix BaseXMatrix::createRotationY(double angle) {
        BaseXMatrix result;
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);
        
        if (result.size >= 3) {
            result[0][0] = cosA;   result[0][2] = sinA;
            result[2][0] = -sinA;  result[2][2] = cosA;
        }
        
        return result;
    }
    
    BaseXMatrix BaseXMatrix::createRotationZ(double angle) {
        BaseXMatrix result;
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);
        
        if (result.size >= 2) {
            result[0][0] = cosA;  result[0][1] = -sinA;
            result[1][0] = sinA;  result[1][1] = cosA;
        }
        
        return result;
    }
    
    BaseXMatrix BaseXMatrix::createScale(double scaleX, double scaleY, double scaleZ) {
        BaseXMatrix result;
        
        if (result.size >= 1) result[0][0] = scaleX;
        if (result.size >= 2) result[1][1] = scaleY;
        if (result.size >= 3) result[2][2] = scaleZ;
        
        // Scale higher dimensions based on quantum effects
        for (int i = 3; i < result.size; i++) {
            result[i][i] = (scaleX + scaleY + scaleZ) / 3.0 * BaseXConfig::getQuantumScale();
        }
        
        return result;
    }
    
    BaseXMatrix BaseXMatrix::createTranslation(const BaseXVector& translation) {
        BaseXMatrix result(BaseXConfig::getDimensions() + 1); // Homogeneous coordinates
        
        // Set translation column
        for (int i = 0; i < translation.getDimensions(); i++) {
            result[i][result.size - 1] = translation[i];
        }
        
        return result;
    }
    
    BaseXMatrix BaseXMatrix::createQuantumRotation(double angle, int dimension1, int dimension2) {
        BaseXMatrix result;
        
        if (dimension1 < result.size && dimension2 < result.size && dimension1 != dimension2) {
            double cosA = std::cos(angle * BaseXConfig::getQuantumScale());
            double sinA = std::sin(angle * BaseXConfig::getQuantumScale());
            
            result[dimension1][dimension1] = cosA;
            result[dimension1][dimension2] = -sinA;
            result[dimension2][dimension1] = sinA;
            result[dimension2][dimension2] = cosA;
        }
        
        return result;
    }
    
    double BaseXMatrix::determinant() const {
        if (size == 1) return matrix[0][0];
        if (size == 2) return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
        if (size == 3) {
            return matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
                   matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
                   matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
        }
        
        // For higher dimensions, return approximation based on diagonal product
        double result = 1.0;
        for (int i = 0; i < size; i++) {
            result *= matrix[i][i];
        }
        return result * BaseXConfig::getQuantumScale();
    }
    
    BaseXMatrix BaseXMatrix::transpose() const {
        BaseXMatrix result(size);
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                result[j][i] = matrix[i][j];
            }
        }
        return result;
    }
    
    void BaseXMatrix::print() const {
        std::cout << "BaseXMatrix (Base-" << BaseXConfig::currentBase << ", " << size << "x" << size << "):" << std::endl;
        for (int i = 0; i < size; i++) {
            std::cout << "[";
            for (int j = 0; j < size; j++) {
                std::cout << matrix[i][j];
                if (j < size - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        }
    }
    
    // BaseXMath namespace implementation
    namespace BaseXMath {
        
        double goldenSpiral(double t, int baseScale) {
            return std::pow(BaseXConfig::PHI, t * baseScale * BaseXConfig::getQuantumScale());
        }
        
        double fibonacciBase(int n) {
            if (n <= 1) return n;
            
            double a = 0, b = 1;
            for (int i = 2; i <= n; i++) {
                double temp = a + b * BaseXConfig::getQuantumScale();
                a = b;
                b = temp;
            }
            return b;
        }
        
        double quantumLerp(double a, double b, double t) {
            double quantumT = t * BaseXConfig::getQuantumScale();
            return a + (b - a) * (quantumT + std::sin(quantumT * BaseXConfig::PHI) * 0.1);
        }
        
        double baseXNoise(double x, double y, double z) {
            double noise = 0.0;
            for (int i = 0; i < BaseXConfig::currentBase; i++) {
                double freq = std::pow(2.0, i);
                double amp = 1.0 / freq;
                noise += std::sin(x * freq + BaseXConfig::PHI) * 
                        std::cos(y * freq - BaseXConfig::PHI) * 
                        std::sin(z * freq) * amp;
            }
            return noise / BaseXConfig::currentBase;
        }
        
        double calculateQuantumPhase(const BaseXVector& position, double time) {
            double phase = time * BaseXConfig::getQuantumScale();
            for (int i = 0; i < position.getDimensions(); i++) {
                phase += position[i] * std::sin(i * BaseXConfig::PHI + time);
            }
            return std::fmod(phase, 2.0 * M_PI);
        }
    }
    
} // namespace BaseXEngine