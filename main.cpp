#include "stdafx.h"
#include <iostream>
#include <pdal/PointTable.hpp>
#include <pdal/PointView.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/Options.hpp>

int main(int argc, char** argv) {
    std::string filePath = "sample.las";
    std::string driver = "readers.las";

    pdal::StageFactory factory;
    pdal::Stage* reader = factory.createStage(driver);

    if (!reader) {
        std::cerr << "오류: " << driver << " 드라이버를 생성할 수 없습니다!" << std::endl;
        return 1;
    }

    pdal::Options options;
    options.add("filename", filePath);
    reader->setOptions(options);

    pdal::PointTable table;
    reader->prepare(table);

    pdal::PointViewSet viewSet = reader->execute(table);

    for (pdal::PointViewPtr view : viewSet) {
        std::cout << "총 읽어온 포인트 개수: " << view->size() << "개" << std::endl;
        
        for (pdal::PointId id = 0; id < 5 && id < view->size(); ++id) {
            double x = view->getFieldAs<double>(pdal::Dimension::Id::X, id);
            double y = view->getFieldAs<double>(pdal::Dimension::Id::Y, id);
            double z = view->getFieldAs<double>(pdal::Dimension::Id::Z, id);

            std::cout << "Point[" << id << "] X:" << x << ", Y:" << y << ", Z:" << z << std::endl;
        }
    }

    return 0;
}