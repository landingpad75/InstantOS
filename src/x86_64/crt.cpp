extern "C" {
    typedef void (*Constructor)(void);

    extern Constructor constructorListStart[];
    extern Constructor constructorListEnd[];

    void initConstructors() {
        for (Constructor* f = constructorListStart; f != constructorListEnd; f++) {
            (*f)();
        }
    }
}
