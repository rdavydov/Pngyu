#include "pngyumainwindow.h"
#include "ui_pngyumainwindow.h"

#include <cmath>

#include <QProcess>
#include <QDebug>
#include <QTime>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include <QDir>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QFileDialog>

#include <QImageReader>

#include "pngyupreviewwindow.h"

#include "pngyu_util.h"
#include "pngyu_execute_compress.h"




namespace
{

enum TableColumn
{
  COLUMN_BASENAME = 0,
  COLUMN_ABSOLUTE_PATH,
  COLUMN_RESULT,
  COLUMN_ORIGINAL_SIZE,
  COLUMN_OUTPUT_SIZE,
  COLUMN_SAVING,
  TABLE_COLUMN_COUNT
};

} // namespace

PngyuMainWindow::PngyuMainWindow(QWidget *parent) :
  QMainWindow( parent ),
  ui( new Ui::PngyuMainWindow ),
  m_preview_window( new PngyuPreviewWindow(this) )
{
  ui->setupUi(this);

  { // init file list table widget
    QTableWidget *table_widget = file_list_table_widget();
    table_widget->setColumnCount( TABLE_COLUMN_COUNT );
    table_widget->setHorizontalHeaderItem( COLUMN_BASENAME,
                                           new QTableWidgetItem( tr("File Name") ) );
    table_widget->setHorizontalHeaderItem( COLUMN_ABSOLUTE_PATH,
                                           new QTableWidgetItem( tr("Path") ) );
    table_widget->setHorizontalHeaderItem( COLUMN_RESULT,
                                           new QTableWidgetItem( tr("Result") ) );
    table_widget->setHorizontalHeaderItem( COLUMN_ORIGINAL_SIZE,
                                           new QTableWidgetItem( tr("Size") ) );
    table_widget->setHorizontalHeaderItem( COLUMN_OUTPUT_SIZE,
                                           new QTableWidgetItem( tr("Compressed Size") ) );
    table_widget->setHorizontalHeaderItem( COLUMN_SAVING,
                                           new QTableWidgetItem( tr("Saving") ) );
  }


  connect( ui->spinBox_colors, SIGNAL(valueChanged(int)), this, SLOT(ncolor_spinbox_changed()) );
  connect( ui->horizontalSlider_colors, SIGNAL(valueChanged(int)), this, SLOT(ncolor_slider_changed()) );


  connect( ui->pushButton_exec, SIGNAL(clicked()), this, SLOT(exec_pushed()) );

  connect( ui->lineEdit_output_directory, SIGNAL(textChanged(QString)), this, SLOT(output_directory_changed()) );

  connect( ui->toolButton_open_output_directory, SIGNAL(clicked()), this, SLOT(open_output_directory_pushed()) );

  connect( ui->radioButton_output_same_directory, SIGNAL(toggled(bool)),
           this, SLOT(output_directory_mode_changed()) );
  //  connect( ui->radioButton_output_other_directory, SIGNAL(toggled(bool)),
  //           this, SLOT(output_directory_mode_changed()) );

  connect( ui->comboBox_output_filename_mode, SIGNAL(currentIndexChanged(int)),
           this, SLOT(output_filename_mode_changed()) );

  connect( ui->pushButton_filelist_clear, SIGNAL(clicked()), this, SLOT(file_list_clear_pushed()) );

  connect( ui->tableWidget_filelist, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(table_widget_current_changed()) );

  connect( ui->pushButton_preview, SIGNAL(toggled(bool)),
           this, SLOT(preview_button_toggled(bool)) );

  connect( m_preview_window, SIGNAL(closed()),
           this, SLOT(preview_window_closed()) );

  // connect compress option changed slot
  connect( ui->spinBox_colors, SIGNAL(valueChanged(int)),
           this, SLOT(compress_option_changed()) );
  connect( ui->horizontalSlider_compress_speed, SIGNAL(valueChanged(int)),
           this, SLOT(compress_option_changed()) );
  connect( ui->checkBox_ie6_support, SIGNAL(stateChanged(int)),
           this, SLOT(compress_option_changed()) );
  connect( ui->checkBox_dithered, SIGNAL(stateChanged(int)),
           this, SLOT(compress_option_changed()) );
  ///

  output_directory_mode_changed();
  output_filename_mode_changed();
  table_widget_current_changed();
  compress_option_changed();

  m_preview_window->set_executable_pngquant_path(
        executable_pngquant_path() );
}

PngyuMainWindow::~PngyuMainWindow()
{
  delete ui;
}

QTableWidget* PngyuMainWindow::file_list_table_widget()
{
  return ui->tableWidget_filelist;
}

void PngyuMainWindow::file_list_clear()
{
  m_file_list.clear();
  update_file_table();
}

QString PngyuMainWindow::make_output_file_path_string( const QString &input_file_path ) const
{
  const QFileInfo input_file_info( input_file_path );
  const QString &prefix = ui->lineEdit_output_file_prefix->text();
  const QString &suffix = ui->lineEdit_output_file_suffix->text();

  const pngyu::OuputDirectoryMode output_dir_mode = current_output_directory_mode();

  QString output_dir_path;
  if( output_dir_mode == pngyu::OUTPUT_DIR_SAME )
  {
    output_dir_path = input_file_info.absolutePath();
  }
  else if( output_dir_mode == pngyu::OUTPUT_DIR_OTHER && is_output_directory_valid() )
  {
    output_dir_path = output_directory();
  }
  else
  {
    return QString();
  }

  return output_dir_path + "/" +
      prefix + input_file_info.baseName() + suffix;
}

pngyu::PngquantOption PngyuMainWindow::make_pngquant_option( const QString &output_file_suffix ) const
{
  pngyu::PngquantOption option;
  option.set_ncolors( ncolor() );

  if( ! output_file_suffix.isEmpty() )
  {
    option.set_out_suffix( output_file_suffix );
  }
  //option.set_force_overwrite( true );
  option.set_speed( compress_speed() );
  option.set_floyd_steinberg_dithering_disabled( ! dither_enabled() );
  option.set_ie6_alpha_support( ie6_alpha_support_enabled() );
  return option;
}

QString PngyuMainWindow::executable_pngquant_path() const
{
  return "/usr/local/bin/pngquant";
}

void PngyuMainWindow::set_current_output_directory_mode( const pngyu::OuputDirectoryMode mode )
{
  if( mode == pngyu::OUTPUT_DIR_SAME )
  {
    ui->radioButton_output_same_directory->setChecked( true );
  }
  else if( mode == pngyu::OUTPUT_DIR_OTHER )
  {
    ui->radioButton_output_other_directory->setChecked( true );
  }
}

pngyu::OuputDirectoryMode PngyuMainWindow::current_output_directory_mode() const
{
  const bool same_checked = ui->radioButton_output_same_directory->isChecked();
  const bool other_checked = ui->radioButton_output_other_directory->isChecked();
  if( same_checked && ! other_checked  )
  {
    return pngyu::OUTPUT_DIR_SAME;
  }
  else if( ! same_checked && other_checked  )
  {
    return pngyu::OUTPUT_DIR_OTHER;
  }
  return pngyu::OUTPUT_DIR_UNKNOWN;
}

void PngyuMainWindow::set_current_outoput_filename_mode( const pngyu::OutputFinenameMode mode )
{
  if( mode == pngyu::OUTPUT_FILE_SAME_AS_ORIGINAL )
  {
    ui->comboBox_output_filename_mode->setCurrentIndex( 0 );
  }
  else if( mode == pngyu::OUTPUT_FILE_CUSTOM )
  {
    ui->comboBox_output_filename_mode->setCurrentIndex( 0 );
  }
}

pngyu::OutputFinenameMode PngyuMainWindow::current_output_filename_mode() const
{
  const int index = ui->comboBox_output_filename_mode->currentIndex();
  if( index == 0 )
  {
    return pngyu::OUTPUT_FILE_SAME_AS_ORIGINAL;
  }
  else if( index == 1 )
  {
    return pngyu::OUTPUT_FILE_CUSTOM;
  }
  return pngyu::OUTPUT_FILE_UNKNOWN;
}

void PngyuMainWindow::set_output_directory( const QString &output_directory )
{
  ui->lineEdit_output_directory->setText( output_directory );
}

QString PngyuMainWindow::output_directory() const
{
  return QDir( ui->lineEdit_output_directory->text() ).absolutePath();
}

void PngyuMainWindow::set_ncolor( const int n )
{
  ui->spinBox_colors->setValue( n );
}

int PngyuMainWindow::ncolor() const
{
  return ui->spinBox_colors->value();
}

void PngyuMainWindow::set_file_overwrite_enabled( const bool b )
{
  ui->checkBox_overwrite->setChecked( b );
}

bool PngyuMainWindow::file_overwrite_enabled() const
{
  return ui->checkBox_overwrite->isChecked();
}

void PngyuMainWindow::set_dither_enabled( const bool b )
{
  ui->checkBox_dithered->setChecked( b );
}

bool PngyuMainWindow::dither_enabled() const
{
  return ui->checkBox_dithered->isChecked();
}

void PngyuMainWindow::set_compress_speed( const int speed )
{
  ui->horizontalSlider_compress_speed->setValue( speed );
}

void PngyuMainWindow::set_ie6_alpha_support_enabled( const bool b )
{
  ui->checkBox_ie6_support->setChecked( b );
}

bool PngyuMainWindow::ie6_alpha_support_enabled() const
{
  return ui->checkBox_ie6_support->isChecked();
}

int PngyuMainWindow::compress_speed() const
{
  return ui->horizontalSlider_compress_speed->value();
}


void PngyuMainWindow::execute_compress_all()
{ 
  const bool overwrite_enable = file_overwrite_enabled();

  const pngyu::PngquantOption &option = make_pngquant_option( QString() );
  const QString &pngquant_path = executable_pngquant_path();

  QTableWidget *table_widget = file_list_table_widget();
  const int row_count = table_widget->rowCount();
  for( int row = 0; row < row_count; ++row )
  {
    const QTableWidgetItem * const absolute_path_item = table_widget->item( row, COLUMN_ABSOLUTE_PATH );
    if( ! absolute_path_item )
    {
      qWarning() << "Item is null. row:" << row;
      continue;
    }

    QTableWidgetItem * const  resultItem = new QTableWidgetItem();
    table_widget->setItem( row, COLUMN_RESULT, resultItem );

    table_widget->scrollToItem( resultItem );

    const QString &src_path = absolute_path_item->text();
    const QString &dst_path = make_output_file_path_string( src_path );

    if( dst_path.isEmpty() )
    {
      qWarning() << "dst path is empty";
      continue;
    }

    const QByteArray &src_png_data = pngyu::util::png_file_to_bytearray( src_path );
    QByteArray dst_png_data;

    const QString command = option.make_pngquant_command_stdio_mode( pngquant_path );

    try
    {
      const bool dst_path_exists = QFile::exists( dst_path );
      if( dst_path_exists && ! overwrite_enable )
      {
        throw QString( "Error: \"" + dst_path + "\" is already exists" );
      }

      const QByteArray &src_png_data = pngyu::util::png_file_to_bytearray( src_path );

      { // exetute pngquant command
        const QPair<QByteArray,QString> &compress_func_res =
            pngyu::execute_compress_stdio_mode( command, src_png_data );
        dst_png_data = compress_func_res.first;
        if( dst_png_data.isEmpty() )
        {
          throw compress_func_res.second;
        }
      }

      if( dst_path_exists )
      {
        if( ! QFile::remove( dst_path ) )
        {
          throw QString( "Error: Couldn't overwrite" );
        }
      }

      // copy result file to dst_path
      if( ! pngyu::util::write_png_data( dst_path, dst_png_data ) )
      {
         throw QString( "Couldn't save output file" );
      }
    }
    catch( const QString &e )
    {
      resultItem->setText( e );
      resultItem->setToolTip( e );
      resultItem->setIcon( pngyu::util::failure_icon() );
      dst_png_data.clear();
    }

    if( ! dst_png_data.isEmpty() )
    {
      resultItem->setIcon( pngyu::util::success_icon() );

      const qint64 src_size = src_png_data.size();
      const qint64 dst_size = dst_png_data.size();

      table_widget->setItem( row, COLUMN_ORIGINAL_SIZE,
                             new QTableWidgetItem( pngyu::util::size_to_string_kb( src_size ) ) );
      table_widget->setItem( row, COLUMN_OUTPUT_SIZE,
                             new QTableWidgetItem( pngyu::util::size_to_string_kb( dst_size ) ) );
      const double saving_rate = static_cast<double>( src_size - dst_size ) / ( src_size );

      table_widget->setItem( row, COLUMN_SAVING,
                             new QTableWidgetItem( QString( "%1%" ).arg( static_cast<int>(saving_rate * 100) ) ) );
    }

    QApplication::processEvents();
  }
}

bool PngyuMainWindow::is_preview_window_visible() const
{
  return m_preview_window->isVisible();
}

QString PngyuMainWindow::current_selected_filename() const
{
  QTableWidget * const table_widget = ui->tableWidget_filelist;
  const int current_row = table_widget->currentRow();
  const QTableWidgetItem * const path_item = table_widget->item( current_row, COLUMN_ABSOLUTE_PATH );
  return path_item ? path_item->text() : QString();
}

//////////////////////
// protected functions
//////////////////////

void PngyuMainWindow::dragEnterEvent( QDragEnterEvent *event )
{
  if( event->mimeData()->hasUrls() )
  {
    event->accept();
  }
}

void PngyuMainWindow::dragLeaveEvent( QDragLeaveEvent *event )
{
  Q_UNUSED(event)
  pngyu::util::set_drop_enabled_palette( ui->centralWidget, false );
  pngyu::util::set_drop_enabled_palette( ui->lineEdit_output_directory, false );
}


void PngyuMainWindow::dragMoveEvent( QDragMoveEvent * event )
{
  Q_UNUSED(event)

  { // linedit
    QLineEdit * const output_line_edit = ui->lineEdit_output_directory;
    pngyu::util::set_drop_enabled_palette(
          output_line_edit,
          pngyu::util::is_under_mouse( output_line_edit ) );
  }

  QWidget * const central_widget = ui->centralWidget;
  pngyu::util::set_drop_enabled_palette(
        central_widget,
        pngyu::util::is_under_mouse( central_widget ) );
}

void PngyuMainWindow::dropEvent( QDropEvent *event )
{
  QLineEdit * const output_line_edit = ui->lineEdit_output_directory;
  const bool mouse_is_on_output = pngyu::util::is_under_mouse( output_line_edit );
  pngyu::util::set_drop_enabled_palette( output_line_edit, false );

  QWidget * const central_widget = ui->centralWidget;
  const bool mouse_is_on_central = pngyu::util::is_under_mouse( central_widget );
  pngyu::util::set_drop_enabled_palette( central_widget, false );

  const QMimeData * const mimedata = event->mimeData();
  if( ! mimedata->hasUrls() )
  {
    return;
  }

  const QList<QUrl> &url_list = mimedata->urls();

  if( mouse_is_on_output )
  {
    if( url_list.size() == 1 )
    {
      set_output_directory( url_list.first().toLocalFile() );
      set_current_output_directory_mode( pngyu::OUTPUT_DIR_OTHER );
    }
  }
  else if( mouse_is_on_central )
  {
    foreach( const QUrl &url, url_list )
    {
      append_file_info_recursive( QFileInfo( url.toLocalFile() ) );
    }
    update_file_table();
  }

}

void PngyuMainWindow::update_file_table()
{
  QTableWidget * const table_widget = file_list_table_widget();
  //temporary disconnect
  disconnect( table_widget, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(table_widget_current_changed()) );
  const QString &last_selected_filename = current_selected_filename();
  table_widget->setRowCount( 0 ); // reset file list table
  table_widget->setRowCount( m_file_list.size() );
  {
    int row_index = 0;
    foreach( const QFileInfo &file_info, m_file_list )
    {
      QTableWidgetItem * const basename_item = new QTableWidgetItem( file_info.baseName() );
      basename_item->setToolTip( file_info.baseName() );
      table_widget->setItem( row_index, COLUMN_BASENAME, basename_item );
      const QString &absolute_path = file_info.absoluteFilePath();
      QTableWidgetItem * const abspath_item =new QTableWidgetItem( file_info.absoluteFilePath() );
      abspath_item->setToolTip( absolute_path );
      table_widget->setItem( row_index, COLUMN_ABSOLUTE_PATH, abspath_item );
      if( last_selected_filename == absolute_path )
      {
        table_widget->setCurrentItem( abspath_item );
      }
      ++row_index;
    }
  }
  // reconnect
  connect( table_widget, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(table_widget_current_changed()) );
  table_widget_current_changed();
}

void PngyuMainWindow::append_file_info_recursive( const QFileInfo &file_info,
                                                  const int recursive_directory_depth )
{
  if( file_info.isFile() )
  { // if file_info is file, png check and add to file_list
    const bool is_png = pngyu::util::has_png_extention( file_info );
    if( is_png && ! m_file_list.contains( file_info ) )
    {
      m_file_list.push_back( file_info );
    }
  }
  else if( file_info.isDir() )
  { // if file_info is dir, call this function recursively by each contents.
    const QDir dir( file_info.absoluteFilePath() );
    foreach( const QFileInfo &child_file_info, dir.entryInfoList( QDir::NoDotAndDotDot | QDir::AllEntries ) )
    {
      append_file_info_recursive( child_file_info, recursive_directory_depth + 1 );
    }
  }
}

bool PngyuMainWindow::is_output_directory_valid() const
{
  QLineEdit * const line_edit = ui->lineEdit_output_directory;
  const QFileInfo output_directory( line_edit->text() );
  const bool valid_directory = output_directory.isDir() && output_directory.exists();
  return valid_directory;
}

////////
// slots
////////

void PngyuMainWindow::exec_pushed()
{
  QTime t;
  t.start();
  execute_compress_all();
  qDebug() << "execute" << t.elapsed() << "ms elapsed";
}

void PngyuMainWindow::compress_option_changed()
{
  m_preview_window->set_current_pngquant_option(
        make_pngquant_option( QString() ) );
}

void PngyuMainWindow::output_directory_changed()
{
  QLineEdit * const line_edit = ui->lineEdit_output_directory;
  QPalette palette = line_edit->palette();
  if( is_output_directory_valid() || output_directory().isEmpty() )
  {
    palette.setBrush( QPalette::Text, QBrush() );
  }
  else
  { // if output directory is invalid, change text color
    palette.setBrush( QPalette::Text, QBrush(Qt::red) );
  }
  line_edit->setPalette( palette );
}

void PngyuMainWindow::open_output_directory_pushed()
{
  const QString path = QFileDialog::getExistingDirectory( this,
                                                          QString(),
                                                          QDir::homePath() );

  if( ! path.isEmpty() )
  {
    set_output_directory( path );
  }
}

void PngyuMainWindow::output_directory_mode_changed()
{
  const pngyu::OuputDirectoryMode mode = current_output_directory_mode();

  ui->toolButton_open_output_directory->setEnabled( mode == pngyu::OUTPUT_DIR_OTHER );
  ui->lineEdit_output_directory->setEnabled( mode == pngyu::OUTPUT_DIR_OTHER );
  QPalette p = ui->lineEdit_output_directory->palette();
  p.setBrush( QPalette::Base, mode == pngyu::OUTPUT_DIR_OTHER ?
                QPalette().brush( QPalette::Base ) : //QBrush(Qt::lightGray) );
                QPalette().brush( QPalette::Window )  );
  ui->lineEdit_output_directory->setPalette( p );
}

void PngyuMainWindow::output_filename_mode_changed()
{
  const pngyu::OutputFinenameMode mode = current_output_filename_mode();
  ui->widget_custom_output_filename->setVisible( mode == pngyu::OUTPUT_FILE_CUSTOM );

  if( mode == pngyu::OUTPUT_FILE_SAME_AS_ORIGINAL &&
      current_output_directory_mode() == pngyu::OUTPUT_DIR_SAME )
    set_file_overwrite_enabled( true );
}

void PngyuMainWindow::file_list_clear_pushed()
{
  file_list_clear();
}

void PngyuMainWindow::ncolor_spinbox_changed()
{
  const int n = ncolor();
  int slider_value = 256;
  if( n < 4 )
    slider_value = 0;
  else if( n < 8 )
    slider_value = 1;
  else if( n < 16 )
    slider_value = 2;
  else if( n < 32 )
    slider_value = 3;
  else if( n < 64 )
    slider_value = 4;
  else if( n < 128 )
    slider_value = 5;
  else if( n < 256 )
    slider_value = 6;
  else
    slider_value = 7;
  ui->horizontalSlider_colors->setValue( slider_value );
}

void PngyuMainWindow::ncolor_slider_changed()
{
  const int slider_value = ui->horizontalSlider_colors->value();
  int ncolor = 256;
  switch( slider_value )
  {
  case 0: ncolor = 2;   break;
  case 1: ncolor = 4;   break;
  case 2: ncolor = 8;   break;
  case 3: ncolor = 16;  break;
  case 4: ncolor = 32;  break;
  case 5: ncolor = 64;  break;
  case 6: ncolor = 128; break;
  case 7: ncolor = 256; break;
  }
  set_ncolor( ncolor );
}

void PngyuMainWindow::table_widget_current_changed()
{
  const QString current_path = current_selected_filename();

  m_preview_window->set_png_file( current_path );

  if( ! current_path.isEmpty() )
  {
    ui->pushButton_preview->setEnabled( true );
    const QSize &icon_size = ui->pushButton_preview->iconSize();
    const QImage &icon_image =
        pngyu::util::read_thumbnail_image( current_path,
                                           icon_size.width() );

    ui->pushButton_preview->setIcon( QPixmap::fromImage( icon_image ) );
  }
  else
  {
    ui->pushButton_preview->setEnabled( false );
    ui->pushButton_preview->setIcon( QIcon() );
  }
}

void PngyuMainWindow::preview_button_toggled( bool b )
{
  m_preview_window->setVisible( b );
}

void PngyuMainWindow::preview_window_closed()
{
  ui->pushButton_preview->setChecked( false );
}
