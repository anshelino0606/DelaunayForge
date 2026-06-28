#include "archive_test.h"
#include "core/object/test.h"

namespace fem {

constexpr uint32_t VALUE1 = 25;
constexpr uint64_t VALUE2 = 126;
constexpr glm::dvec3 VALUE3 = {2.42342423424, 4.52354262346, 8.623462346236};
const std::string VALUE4 = "Archive Test";
const std::vector<float> VALUE5 = {1.5f, 2.5f, 3.7f};
const std::vector<std::vector<glm::ivec3>> VALUE6 = {
    { glm::ivec3(-3), glm::ivec3(-2), glm::ivec3(-1) },
    { glm::ivec3(0), glm::ivec3(1), glm::ivec3(2) },
    { glm::ivec3(3), glm::ivec3(4), glm::ivec3(5) },
};
const std::string PATH = "archive_test.femasset";
const std::string PATH2 = "archive_test_obj_serializetion.femasset";
const std::string PATH3 = "archive_test_obj_arr_serializetion.femasset";

void ArchiveTest::run()
{
    Archive write_archive;
    write_archive << VALUE1 << VALUE2 << VALUE3 << VALUE4 << VALUE5 << VALUE6;
    write_archive.save(PATH);

    Archive read_header_archive(PATH, Archive::Mode::READ_HEADER_ONLY);
    assert(read_header_archive.is_read_header_only_mode());

    Archive read_archive(PATH, Archive::Mode::READ);
    assert(read_archive.is_read_mode());
    uint32_t read_value1;
    uint64_t read_value2;
    glm::dvec3 read_value3;
    std::string read_value4;
    std::vector<float> read_value5;
    std::vector<std::vector<glm::ivec3>> read_value6;
    read_archive >> read_value1 >> read_value2 >> read_value3 >> read_value4 >> read_value5 >> read_value6;

    assert(VALUE1 == read_value1);
    assert(VALUE2 == read_value2);
    assert(VALUE3.x == read_value3.x && VALUE3.y == read_value3.y && VALUE3.z == read_value3.z);
    assert(VALUE4 == read_value4);
    assert(VALUE5 == read_value5);
    assert(VALUE6 == read_value6);

    TestObjectStructObj* struct_obj_write = new TestObjectStructObj();
    struct_obj_write->struct_.struct_value2 = glm::vec3(142);
    struct_obj_write->enum_ = TestEnum::Test4;

    Archive obj_archive_write;
    struct_obj_write->serialize(obj_archive_write);
    obj_archive_write.save(PATH2);

    Archive obj_archive_read(PATH2, Archive::Mode::READ);
    TestObjectStructObj* struct_obj_read = create_object<TestObjectStructObj>(obj_archive_read);

    assert(struct_obj_write->is_equal(struct_obj_read));


    TestObjectArrays* struct_obj_arrs_write = new TestObjectArrays();

    for (uint32_t i = 0; i != 10; ++i) {
        struct_obj_arrs_write->struct_arr_.push_back(PlaceholderStruct{
            .struct_value1 = glm::vec2(static_cast<float>((i + 1) * 2)),
            .struct_value2 = glm::vec3(static_cast<float>((i + 2) * 2))
        });

        PlaceholderObject* temp_obj = new PlaceholderObject();
        temp_obj->set_value(i * 10);
        struct_obj_arrs_write->obj_arr_.push_back(temp_obj);

        struct_obj_arrs_write->vec2_arr_.push_back(glm::vec2(static_cast<float>(i)));
    }

    Archive obj_arr_write_archive;
    struct_obj_arrs_write->serialize(obj_arr_write_archive);
    obj_arr_write_archive.save(PATH3);

    Archive obj_arr_read_archive(PATH3, Archive::Mode::READ);

    TestObjectArrays* struct_obj_arrs_read = create_object<TestObjectArrays>(obj_arr_read_archive);

    for (uint32_t i = 0; i != 10; ++i) {
        assert(struct_obj_arrs_write->struct_arr_[i].struct_value1 
            == struct_obj_arrs_read->struct_arr_[i].struct_value1);
        
        assert(struct_obj_arrs_write->struct_arr_[i].struct_value2 
            == struct_obj_arrs_read->struct_arr_[i].struct_value2);

        assert(struct_obj_arrs_write->obj_arr_[i]->get_value() 
            == struct_obj_arrs_read->obj_arr_[i]->get_value());

        assert(struct_obj_arrs_write->vec2_arr_[i] 
            == struct_obj_arrs_read->vec2_arr_[i]);
    }
}

}