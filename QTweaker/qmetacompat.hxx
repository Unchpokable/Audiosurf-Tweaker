#ifndef QMETACOMPAT_HXX
#define QMETACOMPAT_HXX

#include "Backend/skin_item.hxx"

/*  THIS IS A VERY BIG AND UGLY HACK DEFINITIONS FILE
 *  THE ELDER EVIL LIVES HERE
 *  AND THIS ELDER EVIL ALLOWS US TO USE QT METASYSTEM
 */

Q_DECLARE_METATYPE(QImageList);

QDataStream& operator<<(QDataStream& stream, const QList<QImage>& list);
QDataStream& operator>>(QDataStream& stream, QList<QImage>& list);

namespace qt::dirty
{
void compat_init();
}

#endif // QMETACOMPAT_HXX
