#pragma once
#ifndef FRAMEWORK_H
#define FRAMEWORK_H

namespace CE::GFramework {
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(double seconds) = 0;
        virtual void draw(double seconds) = 0;
    };
}
#endif //FRAMEWORK_H
