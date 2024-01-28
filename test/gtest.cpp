#include <gtest/gtest.h>
#include <fstream>
#include <string>

bool compareFiles(const std::string& file1, const std::string& file2) {
    std::ifstream fileStream1(file1);
    std::ifstream fileStream2(file2);

    std::string line1, line2;
    while (std::getline(fileStream1, line1) && std::getline(fileStream2, line2)) {
        if (line1 != line2) {
            return false;
        }
    }
    return true;
}

TEST(FileComparisonTest, CompareSelect) {
    std::string outputFile = "/tmp/selectOutput.txt";
    std::string answerFile = "selectAnswer.txt";

    EXPECT_TRUE(compareFiles(outputFile, answerFile));
}
TEST(FileComparisonTest, ComparePoll) {
    std::string outputFile = "/tmp/selectOutput.txt";
    std::string answerFile = "pollAnswer.txt";

    EXPECT_TRUE(compareFiles(outputFile, answerFile));
}
TEST(FileComparisonTest, CompareEpoll) {
    std::string outputFile = "/tmp/selectOutput.txt";
    std::string answerFile = "epollAnswer.txt";

    EXPECT_TRUE(compareFiles(outputFile, answerFile));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}