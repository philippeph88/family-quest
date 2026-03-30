FROM gcc:latest
RUN apt-get update && apt-get install -y cmake
WORKDIR /usr/src/app
COPY . .
RUN g++ -std=c++17 main.cpp -o family_app -lpthread
CMD ["./family_app"]
