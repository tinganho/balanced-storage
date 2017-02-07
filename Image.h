#include <iostream>
#include <algorithm>
#include <string>
#include <sstream>
#include <new>
#include <math.h>
#include <vector>

using namespace std;


typedef unsigned long ULONG;

class Image{

public:
    explicit Image(ULONG w = 0, ULONG h = 0)
    :_is_grouped(false),_width(w), _height(h),_index(++NR_OF_IMAGES)
    {}

    virtual ~Image()
    {}

    virtual ULONG calculate_size(ULONG,ULONG) const = 0;
    virtual ULONG getSize();
    int get_index() const { return _index;}

    bool _is_grouped;

protected:

    void calc_pyramid_levels(ULONG w, ULONG h);
    ULONG get_pyramid_levels() const { return round( _result_pyramid_levels ); }
    void reset_pyramid_levels() { _result_pyramid_levels = 0.0; }

    ULONG _width, _height;
    const uint8_t MIN_SIZE = 128;
    double _result_pyramid_levels = 0.0;

private:
    static int NR_OF_IMAGES;
    const int _index;
};

int Image::NR_OF_IMAGES = 0;



class Baseline : public Image
{
public:
    explicit Baseline(ULONG w = 0, ULONG h = 0)
    :Image(w,h)
    {}

    ~Baseline()
    {}

    ULONG getSize() override;
    ULONG calculate_size(ULONG w,ULONG h) const override;

private:
    const double SCALE_FACTOR = 0.2;
};



class JP2 : public Image
{
public:
    explicit JP2(ULONG w = 0, ULONG h = 0)
    :Image(w,h)
    {}

    ~JP2()
    {}

    ULONG calculate_size(ULONG w ,ULONG h) const override;

private:
    const double SCALE_FACTOR = 0.4;
    const short HEIGHT_FACTOR = 16;

};


class BMP : public Image
{
public:
    explicit BMP(ULONG w = 0, ULONG h = 0)
    :Image(w,h)
    {}

    ~BMP()
    {}

    ULONG calculate_size(ULONG w,ULONG h) const override;
    ULONG getSize() override;

};


class Image_Manager
{
public:
    explicit Image_Manager()
    :_groupImages(false)
    {}

    ~Image_Manager()
    {}

    long calculate_stack_compression();

    bool _groupImages;
    vector<int> _current_indeces;
    vector<Image*> _images;

private:
    const int COMPRESSION_FACTOR = 3;
};



ULONG BMP::calculate_size(ULONG w,ULONG h) const
{
    return w * h;
}

ULONG JP2::calculate_size(ULONG w ,ULONG h) const
{
    double tmp_result = (w * h * SCALE_FACTOR) /( log( log(w * h + HEIGHT_FACTOR) ) );
    return (ULONG)round(tmp_result);
}

ULONG Baseline::calculate_size(ULONG w,ULONG h) const
{
    return (ULONG)round(w * h * SCALE_FACTOR);
}

ULONG Image::getSize() // used by JP2, no addition by pyramid levels
{
    return calculate_size(_width, _height);

}

ULONG Baseline::getSize()
{
    reset_pyramid_levels();
    calc_pyramid_levels(_width, _height);
    return (calculate_size(_width, _height) + get_pyramid_levels());
}

ULONG BMP::getSize()
{
    reset_pyramid_levels();
    calc_pyramid_levels(_width, _height);
    return (calculate_size(_width, _height) + get_pyramid_levels() );
}

void Image::calc_pyramid_levels(ULONG w, ULONG h)
{
    ULONG tmpWidth = round(w/2);
    ULONG tmpHeight = round(h/2);

    if( ( tmpWidth > MIN_SIZE) && ( tmpHeight > MIN_SIZE) ) // recurse and add to total
    {
        _result_pyramid_levels += calculate_size(tmpWidth, tmpHeight);
        calc_pyramid_levels(tmpWidth, tmpHeight);
    }
}

long Image_Manager::calculate_stack_compression()
{
    double tot_result = 0.0, subtract_result = 0.0;
    uint16_t local_counter = 0;

    for(size_t i = 0; i < _current_indeces.size(); ++i)
    {
        for(size_t j = 0; j < _images.size(); ++j)
        {
            if( (_current_indeces.at(i)  == _images.at(j)->get_index() ) && !(_images.at(j)->_is_grouped) )
            {
                cout<<"["<<_current_indeces.at(i)<<"]  size: "<<_images.at(j)->getSize()<<endl;
                tot_result += _images.at(j)->getSize();
                local_counter++;
                _images.at(j)->_is_grouped = true;
            }
        }
    }

    subtract_result = tot_result;
    tot_result = round(tot_result / log( local_counter + COMPRESSION_FACTOR ));

    cout<<endl<<"previous size of images: "<< subtract_result <<endl;
    cout<<"total compressed size: "<< tot_result<<endl<<endl;

    tot_result -= subtract_result;
    _current_indeces.clear();

    return (long)round(tot_result);
}
