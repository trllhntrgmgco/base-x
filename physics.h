#pragma once
#include <vector>
#include <cmath>
#include <memory>
#include <algorithm>
#include <iostream>

// Base-X Game Engine - Phase 1: Core Mathematical Foundation
// Copyright 2025 - Keenan Dunham

namespace BaseXEngine {
    
    // Core configuration for Base-X system
    class BaseXConfig {
    public:
        static int currentBase;
        static constexpr int MIN_BASE = 4;
        static constexpr int MAX_BASE = 64;
        static constexpr double PHI = 1.618033988749; // Golden ratio for quantum calculations
        
        static void setBase(int newBase);
        static int getBase();
        static int getDimensions();
        static double getQuantumScale();
    };

    // Base-X Vector class supporting N-dimensional calculations
    class BaseXVector {
    private:
        std::vector<double> components;
        int dimensions;
        
    public:
        // Constructors
        BaseXVector();
        BaseXVector(double x, double y, double z);
        BaseXVector(const std::vector<double>& values);
        
        // Accessors for standard 3D coordinates
        double x() const;
        double y() const;
        double z() const;
        
        void setX(double value);
        void setY(double value);
        void setZ(double value);
        
        // N-dimensional accessors
        double operator[](int index) const;
        double& operator[](int index);
        
        int getDimensions() const;
        
        // Vector arithmetic operations
        BaseXVector operator+(const BaseXVector& other) const;
        BaseXVector operator-(const BaseXVector& other) const;
        BaseXVector operator*(double scalar) const;
        BaseXVector operator/(double scalar) const;
        
        BaseXVector& operator+=(const BaseXVector& other);
        BaseXVector& operator-=(const BaseXVector& other);
        BaseXVector& operator*=(double scalar);
        BaseXVector& operator/=(double scalar);
        
        // Vector operations
        double dot(const BaseXVector& other) const;
        BaseXVector cross(const BaseXVector& other) const;
        
        double magnitude() const;
        double magnitudeSquared() const;
        
        BaseXVector normalized() const;
        void normalize();
        
        double distanceTo(const BaseXVector& other) const;
        double distanceSquaredTo(const BaseXVector& other) const;
        
        // Quantum effects based on Base-X
        double getQuantumPhase() const;
        bool isQuantumEntangled(const BaseXVector& other) const;
        
        // Base-X specific operations
        BaseXVector getBaseXProjection(int targetBase) const;
        
        // Debug output
        void print() const;
    };

    // Base-X Matrix class for transformations
    class BaseXMatrix {
    private:
        std::vector<std::vector<double>> matrix;
        int size;
        
    public:
        // Constructors
        BaseXMatrix();
        BaseXMatrix(int dimensions);
        
        // Access operators
        std::vector<double>& operator[](int row);
        const std::vector<double>& operator[](int row) const;
        
        // Matrix operations
        BaseXMatrix operator*(const BaseXMatrix& other) const;
        BaseXVector transform(const BaseXVector& vec) const;
        
        // Common transformation matrices
        static BaseXMatrix createRotationX(double angle);
        static BaseXMatrix createRotationY(double angle);
        static BaseXMatrix createRotationZ(double angle);
        static BaseXMatrix createScale(double scaleX, double scaleY, double scaleZ);
        static BaseXMatrix createTranslation(const BaseXVector& translation);
        
        // Base-X specific transformations
        static BaseXMatrix createQuantumRotation(double angle, int dimension1, int dimension2);
        
        // Matrix properties
        double determinant() const;
        BaseXMatrix transpose() const;
        
        void print() const;
    };

    // Utility functions for Base-X mathematics
    namespace BaseXMath {
        
        // Golden ratio based calculations
        double goldenSpiral(double t, int baseScale = 1);
        
        // Base-X fibonacci sequence
        double fibonacciBase(int n);
        
        // Quantum interpolation
        double quantumLerp(double a, double b, double t);
        
        // Base-X noise function
        double baseXNoise(double x, double y, double z);
        
        // Quantum phase calculation
        double calculateQuantumPhase(const BaseXVector& position, double time);
    }
    
} // namespace BaseXEngine