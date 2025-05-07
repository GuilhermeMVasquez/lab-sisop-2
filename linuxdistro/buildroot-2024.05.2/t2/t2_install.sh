#!/bin/bash

cd ..

if [ ! -d "modules" ]; then
    echo "Creating modules directory..."
    mkdir -p modules
fi

cd modules

if [ -d "mq_driver" ]; then
    echo "Removing existing mq_driver..."
    rm -rf mq_driver/*
else
    echo "Creating mq_driver directory..."
    mkdir -p mq_driver
fi

echo "Moving files to mq_driver directory..."
cp ../t2/mq_driver/* mq_driver/

cd ..

if [ ! -d "custom-scripts" ]; then
    echo "Creating custom-scripts directory..."
    mkdir -p custom-scripts
fi

cd custom-scripts

if [ ! -f "pre-build.sh" ]; then
    echo "Creating pre-build.sh..."
    touch pre-build.sh
    chmod +x pre-build.sh
    echo "#!/bin/bash" > pre-build.sh
    echo "# Pre-build script for custom configurations" >> pre-build.sh
fi

if ! grep -q "make -C \$BASE_DIR/../modules/mq_driver" pre-build.sh; then
    echo "# Trabalho 2" >> pre-build.sh
    echo "make -C \$BASE_DIR/../modules/mq_driver/" >> pre-build.sh
fi

cd ..

if [ ! -d "apps" ]; then
    echo "Creating apps directory..."
    mkdir -p apps
fi

cd apps

if [ ! -d "test_mq_driver" ]; then
    echo "Creating test_mq_driver directory..."
    mkdir -p test_mq_driver
else
    rm -rf test_mq_driver/*
fi

echo "Moving application files to apps directory..."
cp ../t2/test_mq_driver/* test_mq_driver/

cd test_mq_driver

make

cd ../..

echo "Installation completed."

make

echo "Build process completed."