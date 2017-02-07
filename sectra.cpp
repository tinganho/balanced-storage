#include "Image.h"

void print_header(ostream&);
Image* extract_input(const string, Image_Manager*);


const string GROUP("g");
const string JPG_1("jpg");
const string JPG_2("j");
const string JP2_1("jp2");
const string JP2_2("jpeg2000");
const string BMP_1("bmp");
const string EXIT("q");


int main()
{
    ULONG _result = 0;
    Image *image = nullptr;
    Image_Manager im;
    string input ="";

    // using std output
    print_header(cout);

    // first read from std input
    getline(cin,input);

    transform(input.begin(), input.end(), input.begin(), ::tolower); // make all string comparisson on lower case

    while(input.compare(EXIT) != 0)
    {
        // process and add to total sum if valid format
        image = extract_input( input, &im );

        if( image )
        {
            _result += image->getSize();
        }

        if( im._groupImages )
        {
            _result += im.calculate_stack_compression();
        }

        getline(cin, input);
        transform(input.begin(), input.end(), input.begin(), ::tolower);
    }

    cout<<endl<<"Total size: "<< _result <<" bytes"<<endl;

    return 0;
}


void print_header(ostream &os)
{
    os<< "Storage calculator by Carlos Palomeque Vela"<<endl
      << "Enter one line for each image/group on the format \"[type] [width] [height]\""<<endl
      << "or \"G i, i, ...\". Exit with \"Q\". Input is not case-sensitive"<<endl<<endl;
}


Image* extract_input(const string input, Image_Manager *im)
{
    istringstream iss(input);
    string type ="";
    ULONG width = 0, height = 0;
    Image *_image = nullptr;
    int group_read = 0;

    im->_groupImages = false;// disable grouping by default

    if( iss >> type ) // read & test
    {
        if(!type.compare(GROUP))
        {
            cout<<"[grouping images]"<<endl;

            while( iss >> group_read )
            {
                im->_current_indeces.push_back(group_read);
            }

            im->_groupImages = true;
        }
        else if ( type.compare(JPG_1) == 0 || type.compare(JPG_2)== 0 )
        {
            if( iss >> width >> height )
            {
                _image = new Baseline(width,height);
                cout<<"[JPEG/Baseline] size: "<< _image->getSize() << "  index: "<<_image->get_index()<<endl<<endl;
                im->_images.push_back(_image);
            }
        }
        else if ( type.compare(JP2_1) == 0 || type.compare(JP2_2) == 0)
        {
            if(iss >> width >> height)
            {
                _image = new JP2(width,height);
                cout<<"[JP2/2000] size: "<< _image->getSize()<<"  index: "<<_image->get_index()<<endl<<endl;
                im->_images.push_back(_image);
            }
        }
        else if( type.compare(BMP_1)== 0)
        {
            if(iss >> width >> height)
            {
                _image = new BMP(width, height);
                cout<<"[BMP] size: "<< _image->getSize() << "  index: "<<_image->get_index()<<endl<<endl;
                im->_images.push_back(_image);
            }
        }
        else
        {
            cout <<"[invalid input]"<<endl;
        }
    }

    return _image;
}

